#include "saturnd.h"

const int IS_DAEMON = 1;

int main(int argc, char *argv[]) 
{
    if (IS_DAEMON){
        run_daemon();
    }
    
    procInfo("Start saturnd ...");

    int              ret = EXIT_SUCCESS;
    struct pollfd    fds[1];
    int              cycle = 0;

    // 1) Create context stContext 
    struct stContext *context = (struct stContext*) malloc(sizeof(struct stContext));
    if (!context) 
    {
        procError("malloc context is failed");
        ret = EXIT_FAILURE;
        goto lExit;
    }    
    memset(context, 0, sizeof(struct stContext));

    context->tasks = (struct listElements_t*) malloc(sizeof(struct listElements_t));
    if (!context->tasks) {
        procError("malloc context->tasks is failed");
        ret = EXIT_FAILURE;
        goto lExit;
    }
    memset(context->tasks, 0, sizeof(struct listElements_t));
    context->exit        = 0;
    context->pipeReply   = -1;
    context->pipeRequest = -1;

    // 2) if not existing: creates 2 pipes /tmp/<USER_NAME>/saturnd/pipes, else open
    // create path 
    context->pipeReqName = createFilePath("pipes" PIPE_REQUEST_NAME);
    context->pipeRepName = createFilePath("pipes" PIPE_REPLY_NAME);
 
    if (!isFileExists(context->pipeReqName->text)){
        if (mkfifo(context->pipeReqName->text, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) < 0){
            procError("make fifo is failure");
            ret = EXIT_FAILURE;
            goto lExit;
        }
    }

    if (!isFileExists(context->pipeRepName->text)){
        if (mkfifo(context->pipeRepName->text, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) < 0){
            procError("make fifo is failure");
            ret = EXIT_FAILURE;
            goto lExit;
        }
    }

    //obtain fd for reply-pipe and request-pipe, using NONBLOCK more for request FIFO to be capable to open it 
    //even if on other side there is no any process

    //IMPORTANT: pipe is opened with O_RDWR instead O_RDONLY for next reason:
    //function **poll** returns with POLLHUP value in fds[0].revents constantly and immediately after first 
    //communication session with Cassini -> cassini close the request pipe and this action force Saturn to
    //return from **poll** all the time with POLLHUP return code.
    //this behavious produce serions CPU load, and to optimize it next hack was used:
    //https://stackoverflow.com/questions/22021253/poll-on-named-pipe-returns-with-pollhup-constantly-and-immediately
    context->pipeRequest = open(context->pipeReqName->text, O_RDWR | O_NONBLOCK);   
    if (context->pipeRequest < 0){
        procError("open fifo is failed");
        ret = EXIT_FAILURE;
        goto lExit;
    }

    procInfo("restorting state ...");

    // 3) Load from disc list of the tasks and store it in context->pTasks
    ret = restoreTasksFromHdd(context);
    if (EXIT_SUCCESS != ret)
    {
        goto lExit;
    }

    procInfo("daemon is waiting for requests");

    // 4) Organize loop
    // - listen request-pipe, if something arrive - read first 2 bytes (OPCODE) and delegate read and execute the rest to 
    //                                              his handler (send the list, create task, etc)   
    // 
    //          read the rest 
    //                          auxilairy function (how match bytes need to reed),
    //                                    which handle timeout and return the part read                                   
    //          put the task to list
    //          execute the task by fork
    //          send the report to reply-pipe
    //        If temrinate = exit loop
    // - from time to time checks timings of tasks in context->pTasks - if need to execute some of them
    fds[0].fd      = context->pipeRequest;
    fds[0].events  = POLLIN;
    fds[0].revents = 0;    

    // while exit-signal doesn't received && have not errors
    while ((0 == context->exit) && (EXIT_SUCCESS == ret)) 
    {
        int pollRet = poll(fds, 1, 500); //2 times per second
        if (pollRet > 0) {
            uint16_t code = 0;
            size_t szRead = read(context->pipeRequest, &code, sizeof(code));
            if (szRead == sizeof(code))
            {
                code = be16toh(code);
                switch (code){
                    case CLIENT_REQUEST_LIST_TASKS              : ret = processListCmd(context); break;
                    case CLIENT_REQUEST_CREATE_TASK             : ret = processCreateCmd(context); break;
                    case CLIENT_REQUEST_REMOVE_TASK             : ret = processRemoveCmd(context); break;
                    case CLIENT_REQUEST_GET_TIMES_AND_EXITCODES : ret = processTimesExitCodesCmd(context); break;
                    case CLIENT_REQUEST_TERMINATE               : ret = processTerminate(context); break;
                    case CLIENT_REQUEST_GET_STDOUT              : ret = processStdOutCmd(context); break;
                    case CLIENT_REQUEST_GET_STDERR              : ret = processStdErrCmd(context); break;
                    default :
                        procError("unknown request!");
                        ret = EXIT_FAILURE;
                        break;
                }
            }

        } else if (pollRet < 0){ 
            procError("poll error");
            ret = EXIT_FAILURE;
            break;
        }

        if (EXIT_SUCCESS == ret){
            ret = maintainTasks(context);
        }
    }

    procInfo("shutting down, saving state");
    if (EXIT_SUCCESS == ret){
        ret = saveTasksToHdd(context);
    }

    procInfo("closing ...");
lExit:
    //clear all list elements
    while(context->tasks->first){
        freeTask((struct stTask *)context->tasks->first->data);
        removeEl(context->tasks, context->tasks->first);
    }

    FREE_MEM(context->tasks); 
    CLOSE_FILE(context->pipeReply);
    CLOSE_FILE(context->pipeRequest);  
    FREE_STR(context->pipeReqName);
    FREE_STR(context->pipeRepName);
    FREE_MEM(context);
    
    procInfo("that's all, see you!");

    closeDeamonLog();

    return ret;
}

// Restore content of context->tasks from disk
int restoreTasksFromHdd(struct stContext *context){
    struct stString  *filePath = createFilePath(TASKS_FILE);
    int exit_value = EXIT_SUCCESS;
    int fileD = -1;
    uint32_t fileSize = 0;
    struct element_t *taskEl  = NULL;
    struct stTask    *task    = NULL;
    struct stTask    *newTask = NULL;

    if(!filePath){
        exit_value = EXIT_FAILURE;
        procError("Can't allocate memory");
        goto lExit;
    }

    fileD = open(filePath->text, O_RDONLY);

    if (fileD < 0){
        procInfo("Can't open file " TASKS_FILE " working without session restoration");
        goto lExit;
    }
    
    //find size of file & jump to beginning
    fileSize = lseek(fileD, 0, SEEK_END);   
    lseek(fileD, 0, SEEK_SET);

    // read all tasks
    while ((fileSize > sizeof(struct stTask)) && (EXIT_SUCCESS == exit_value)){
        newTask = (struct stTask*) malloc(sizeof(struct stTask)); 
        if (!newTask){
            procError("memory allocation");
            exit_value = EXIT_FAILURE;
            break;
        }

        if (sizeof(struct stTask) != read(fileD, newTask, sizeof(struct stTask))){
            procError("can't read data");
            exit_value = EXIT_FAILURE;
            break;
        }
        fileSize -= sizeof(struct stTask);

        //read task arguments
        newTask->argV = (struct stString **)malloc(sizeof(struct stString *) * newTask->argC);
        if (!newTask->argV){
            procError("memory allocation");
            exit_value = EXIT_FAILURE;
            break;
        }

        //read all command's arguments
        for (size_t i = 0; i < newTask->argC; i++) {
            uint32_t strLen = 0;

            if (sizeof(strLen) != read(fileD, &strLen, sizeof(strLen))){
                procError("can't read data");
                exit_value = EXIT_FAILURE;
                break;
            }
            newTask->argV[i] = createStringBuffer(strLen);

            if (newTask->argV[i]->len != read(fileD, newTask->argV[i]->text, newTask->argV[i]->len)){
                procError("read arg text");
                exit_value = EXIT_FAILURE;
                break;
            }

            newTask->argV[i]->text[strLen] = 0;         //string must be terminated by 0
            fileSize -= (sizeof(strLen) + newTask->argV[i]->len);
        }

        if (exit_value != EXIT_SUCCESS){
            break;
        }

        //read all runs
        newTask->runs = (struct listElements_t *)malloc(sizeof(struct listElements_t));
        if (!newTask->runs){
            procError("memory allocation");
            exit_value = EXIT_FAILURE;
            break;
        }
        memset(newTask->runs, 0, sizeof(struct listElements_t));
            
        uint32_t runsCount = 0;    
        if (sizeof(runsCount) != read(fileD, &runsCount, sizeof(runsCount))){
            procError("can't read data");
            exit_value = EXIT_FAILURE;
            break;
        }

        fileSize -= sizeof(runsCount);

        for (uint32_t i = 0; i < runsCount; i++){
            struct stRunStat* run = (struct stRunStat*)malloc(sizeof(struct stRunStat));
            if (run){
                if (sizeof(struct stRunStat) == read(fileD, run, sizeof(struct stRunStat))){
                    pushLast(newTask->runs, run);
                    fileSize -= sizeof(struct stRunStat);
                }else{
                    procError("can't read data");
                    exit_value = EXIT_FAILURE;
                    free(run);
                    break;
                }
            } else {
                procError("memory allocation");
                exit_value = EXIT_FAILURE;
                break;
            }
        }

        //task was read entirely -> push to list of tasks
        if (EXIT_SUCCESS == exit_value){
            pushLast(context->tasks, newTask);
        }
    }

    // set lastTaskId
    if (EXIT_SUCCESS == exit_value){
        struct element_t *taskEl  = context->tasks->first;
        struct stTask    *task    = NULL;
        uint64_t          maxTaskId = 0;
        while ((taskEl) && (EXIT_SUCCESS == exit_value)){
            task = (struct stTask *)(taskEl->data);
            if (task->taskId > maxTaskId){
                maxTaskId = task->taskId;
            }
            taskEl = taskEl->next;    
        }        
        context->lastTaskId = maxTaskId;
    } else {
        freeTask(newTask);
        goto lExit;
    }

lExit:
    freeString(filePath);
    CLOSE_FILE(fileD);
    return exit_value;
}


// Save content of context->tasks to disk
int saveTasksToHdd(struct stContext *context){
    struct stString  *filePath = createFilePath(TASKS_FILE);
    int exit_value = EXIT_SUCCESS;
    int fileD = -1;
    struct element_t *taskEl  = NULL;
    struct stTask    *task    = NULL;

    if(!filePath){
        exit_value = EXIT_FAILURE;
        procError("Can't allocate memory");
        goto lExit;
    }

    // WARNING: context->tasks will be written by following pattern:
    // for each task: struct stTask + argC + argV + count of runs + runs
    fileD = open(filePath->text, O_CREAT | O_RDWR | O_TRUNC,
                                 S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH);

    if (fileD < 0){
        exit_value = EXIT_FAILURE;
        procError("Can't open file");
        goto lExit;
    }

    taskEl = context->tasks->first;
    while ((taskEl) && (EXIT_SUCCESS == exit_value)){
        task = (struct stTask *)(taskEl->data);
        //make copy to save
        struct stTask taskCopy = *task;
        taskCopy.argV = NULL;
        taskCopy.stdOut = -1;
        taskCopy.stdErr = -1;
        taskCopy.lastPid = 0;
        taskCopy.runs = NULL;
        //save task copy
        if (sizeof(struct stTask) != write(fileD, &taskCopy, sizeof(struct stTask))){
            exit_value = EXIT_FAILURE;
            procError("Can't write file");
            break;
        }

        //save task arguments
        for (size_t i = 0; i < task->argC; i++){
            uint32_t size = (uint32_t)task->argV[i]->len;
            if (sizeof(uint32_t) != write(fileD, &size, sizeof(size))){
                exit_value = EXIT_FAILURE;
                procError("Can't write file");
                break;
            }
            if (task->argV[i]->len != write(fileD, task->argV[i]->text, task->argV[i]->len)){
                exit_value = EXIT_FAILURE;
                procError("Can't write file");
                break;
            }
        }

        if (exit_value != EXIT_SUCCESS){
            break;
        }

        //save task runs
        if (sizeof(task->runs->count) != write(fileD, &task->runs->count, sizeof(task->runs->count))){
            exit_value = EXIT_FAILURE;
            procError("Can't write file");
            break;
        }

        struct element_t *it = task->runs->first;
        while (it){
            struct stRunStat *run = (struct stRunStat *)it->data;

            if (sizeof(struct stRunStat) != write(fileD, run, sizeof(struct stRunStat))){
                exit_value = EXIT_FAILURE;
                procError("Can't write file");
                break;
            }
            it = it->next;
        }

        taskEl = taskEl->next;
    }

    if (exit_value != EXIT_SUCCESS){
        goto lExit;
    }

lExit:

    freeString(filePath);
    CLOSE_FILE(fileD);
    return exit_value;
}


int processListCmd(struct stContext *context){

    int exit_value = EXIT_SUCCESS;

    //Header
    uint16_t opCode = htobe16(SERVER_REPLY_OK);
    uint32_t nbTask = htobe32(context->tasks->count);
    //Position
    int acc=sizeof(uint16_t)+ sizeof(uint32_t);
    char *buf= malloc(acc);
    if (buf == NULL){
        procError("Error realloc");
        exit_value= EXIT_FAILURE;
        goto lExit;
    }
    memmove(buf, &opCode, sizeof(uint16_t));
    memmove(buf+ sizeof(uint16_t), &nbTask, sizeof(uint32_t));

    //if 0 Task
    if (context->tasks->count == 0){
        goto lExit;
    }

    //Task Loop
    struct element_t *e;
    struct stTask *task;
    for (e=context->tasks->first; e!=NULL; e=e->next){
        task = (struct stTask *)(e->data);

        //ID
        uint64_t idTask = htobe64(task->taskId);
        //Timing
        uint64_t minutes = htobe64(task->min);
        uint32_t hours = htobe32(task->heu);
        uint8_t days = task->day;

        //Task
        char bufTaskIDTiming[sizeof(uint64_t)+sizeof(uint64_t)+sizeof(uint32_t)+ sizeof(uint8_t)];
        memmove(bufTaskIDTiming, &idTask, sizeof(uint64_t));
        memmove(bufTaskIDTiming+ sizeof(uint64_t), &minutes, sizeof(uint64_t));
        memmove(bufTaskIDTiming+ sizeof(uint64_t)+ sizeof(uint64_t), &hours, sizeof(uint32_t));
        memmove(bufTaskIDTiming+ sizeof(uint64_t)+ sizeof(uint64_t)+ sizeof(uint32_t), &days, sizeof(uint8_t));

        //Command Line
        int sizeBuf= sizeof(uint32_t);
        char *commandLineBuf = malloc(sizeBuf);
        if (commandLineBuf==NULL){
            procError("fail malloc");
            exit_value=EXIT_FAILURE;
        }
        uint32_t nbrArg = htobe32(task->argC);
        memmove(commandLineBuf, &nbrArg, sizeof(uint32_t));

        //ARGV
        for (size_t x = 0; x < task->argC; x++){

            sizeBuf+=sizeof(uint32_t) + task->argV[x]->len;

            commandLineBuf = realloc(commandLineBuf, sizeBuf);
            if (commandLineBuf == NULL){
                procError("Error realloc");
                exit_value= EXIT_FAILURE;
                goto lExit;
            }
            uint32_t lengthW = htobe32(task->argV[x]->len);
            char *dataWord = (task->argV[x]->text);

            memmove(commandLineBuf+sizeBuf-sizeof(uint32_t)-task->argV[x]->len, &lengthW, sizeof(uint32_t));
            memmove(commandLineBuf+sizeBuf-task->argV[x]->len, (uint8_t *)dataWord, task->argV[x]->len);
        }

        //Position
        acc+=sizeof(uint64_t)+sizeof(uint64_t)+sizeof(uint32_t)+ sizeof(uint8_t)+/*Task*/ sizeBuf/*Command Line*/;

        buf = realloc(buf, acc);
        if (buf == NULL){
            procError("Error realloc");
            exit_value= EXIT_FAILURE;
            goto lExit;
        }

        //Add Task I
        int bonDec =
                acc
                -(sizeof(uint64_t)+ sizeof(uint64_t)+ sizeof(uint32_t)+ sizeof(uint8_t)+sizeBuf)
                ;

        memmove(buf+ bonDec, bufTaskIDTiming, sizeof(uint64_t)+sizeof(uint64_t)+sizeof(uint32_t)+ sizeof(uint8_t));
        memmove(buf+ bonDec + sizeof(uint64_t)+sizeof(uint64_t)+sizeof(uint32_t)+ sizeof(uint8_t), commandLineBuf, sizeBuf);
        free(commandLineBuf);
    }

    lExit:
        if (EXIT_SUCCESS == exit_value){
            writeReply(context, (uint8_t*)buf, acc);
        }
        free(buf);
        return exit_value;
}

int processCreateCmd(struct stContext *context){

    int            retCode = EXIT_SUCCESS;
    uint64_t       min     = 0;
    uint32_t       hours   = 0;
    uint8_t        days    = 0;
    uint32_t       argc    = 0;
    ssize_t        rezRead = 0;
    time_t         curTime = time(NULL);
    const size_t   replySz = sizeof(uint16_t) + sizeof(uint64_t);
    uint8_t        replyBuf[replySz];

    //create structure for new task
    struct stTask *newTask = (struct stTask*) malloc(sizeof(struct stTask)); 
    if (!newTask){
        procError("memory allocation");
        retCode = EXIT_FAILURE;
        goto lExit;
    }

    newTask->runs = (struct listElements_t *)malloc(sizeof(struct listElements_t));
    if (!newTask->runs){
        procError("memory allocation");
        retCode = EXIT_FAILURE;
        goto lExit;
    }
    memset(newTask->runs, 0, sizeof(struct listElements_t));

    // read timing
    rezRead = read(context->pipeRequest, &min, sizeof(min));
    if (rezRead < sizeof(min)){
        procError("read minutes");
        retCode = EXIT_FAILURE;
        goto lExit;
    }
    min = be64toh(min);

    rezRead = read(context->pipeRequest, &hours, sizeof(hours));
    if (rezRead < sizeof(hours)){
        procError("read hours");
        retCode = EXIT_FAILURE;
        goto lExit;
    }
    hours = be32toh(hours);

    rezRead = read(context->pipeRequest, &days, sizeof(days));
    if (rezRead < sizeof(days)){
        procError("read daysOfWeek");
        retCode = EXIT_FAILURE;
        goto lExit;
    }

    // fill newTask daysOfWeek, hours, minutes 
    newTask->day=days;
    for (int i = 0; i < 7; i++){
        if (days & (1 << i)) {
            newTask->daysOfWeek[i] = 1;
        } else {
            newTask->daysOfWeek[i] = 0;
        }
    }

    newTask->heu=hours;
    for (int i = 0; i < 24; i++){
        if (hours & (1 << i)){
            newTask->hours[i] = 1;
        } else {
            newTask->hours[i] = 0;
        }
    }

    newTask->min=min;
    for (int i = 0; i < 60; i++) {
        if (min & (1 << i)) {
            newTask->minutes[i] = 1;
        } else {
            newTask->minutes[i] = 0;
        }
    }

    // fill argC
    rezRead = read(context->pipeRequest, &argc, sizeof(argc));
    if (rezRead < sizeof(argc)){
        procError("read argc");
        retCode = EXIT_FAILURE;
        goto lExit;
    }
    argc = be32toh(argc);
    newTask->argC = (size_t)argc;

    newTask->argV = (struct stString **)malloc(sizeof(struct stString *) * newTask->argC);
    if (!newTask->argV){
        procError("memory allocation");
        retCode = EXIT_FAILURE;
        goto lExit;
    }

    for (size_t i = 0; i < newTask->argC; i++) //read all command's arguments
    {
        int strLen = 0;
        rezRead = read(context->pipeRequest, &strLen, sizeof(strLen));
        if (rezRead < sizeof(strLen))
        {
            procError("read argc");
            retCode = EXIT_FAILURE;
            break;
        }
        strLen = be32toh(strLen); 
        newTask->argV[i] = createStringBuffer(strLen);

        rezRead = read(context->pipeRequest, newTask->argV[i]->text, newTask->argV[i]->len);
        if (rezRead < newTask->argV[i]->len)
        {
            procError("read arg text");
            retCode = EXIT_FAILURE;
            break;
        }

        newTask->argV[i]->text[strLen] = 0;         //string must be terminated by 0
    }

    if (retCode != EXIT_SUCCESS)
    {
        goto lExit;
    }

    //fill task ID
    newTask->taskId = ++context->lastTaskId;

    //fill time of creation
    newTask->stCreated = *localtime(&curTime);

    //set last execution time in the past, to let function maintainTasks as soon as possible regarding schedule
    curTime -= 59; //minus 59 seconds    
    newTask->stExecuted = *localtime(&curTime);

    //init pid of process by 0
    newTask->lastPid    = 0;

    *(uint16_t*)replyBuf = htobe16(SERVER_REPLY_OK);
    *(uint64_t*)(replyBuf + sizeof(uint16_t)) = htobe64(newTask->taskId);
    retCode = writeReply(context, replyBuf, replySz);

lExit:
    if (EXIT_SUCCESS == retCode){
        pushLast(context->tasks, newTask);
    } else {
        freeTask(newTask);
        newTask = NULL;
    }

    return retCode;
}

int processRemoveCmd(struct stContext *context){
    int               ret     = EXIT_SUCCESS;
    uint64_t          taskId  = 0;
    ssize_t           rezRead = 0;
    struct element_t *taskEl  = context->tasks->first;
    struct stTask    *task    = NULL;
    const size_t      replySz = 2 * sizeof(uint16_t);
    uint8_t           replyBuf[replySz];
    char              txtBuf[256];
    char              txtPath[4096];

    // read task ID
    rezRead = read(context->pipeRequest, &taskId, sizeof(taskId));
    if (rezRead < sizeof(taskId)){
        procError("read task ID");
        ret = EXIT_FAILURE;
        goto lExit;
    }
    taskId = be64toh(taskId);

    //find required task 
    while ((taskEl) && (EXIT_SUCCESS == ret))
    {
        task = (struct stTask *)(taskEl->data);
        if (task->taskId == taskId)
        {
            break;
        }
        taskEl = taskEl->next;
    }

    if (taskEl)
    {
        sprintf(txtBuf, "/tasks/%lu/%s", task->taskId, TASK_STD_OUT_NAME);
        GET_DEFAULT_PATH(txtPath, txtBuf);
        remove(txtPath);

        sprintf(txtBuf, "/tasks/%lu/%s", task->taskId, TASK_STD_ERR_NAME);
        GET_DEFAULT_PATH(txtPath, txtBuf);
        remove(txtPath);

        sprintf(txtBuf, "/tasks/%lu", task->taskId);
        GET_DEFAULT_PATH(txtPath, txtBuf);
        rmdir(txtPath);

        removeEl(context->tasks, taskEl);
        freeTask(task);

        *(uint16_t*)replyBuf = htobe16(SERVER_REPLY_OK);
        ret = writeReply(context, replyBuf, sizeof(uint16_t));

    }
    else
    {
        *(uint16_t*)replyBuf = htobe16(SERVER_REPLY_ERROR);
        *(uint16_t*)(replyBuf + sizeof(uint16_t)) = htobe16(SERVER_REPLY_ERROR_NOT_FOUND);
        ret = writeReply(context, replyBuf, 2 * sizeof(uint16_t));
    }

lExit:
    return ret;
}

int processTimesExitCodesCmd(struct stContext *context){

    uint64_t taskIdRq = 0;
    int exit_value = EXIT_SUCCESS;

    //TaskId
    size_t rezRead = read(context->pipeRequest, &taskIdRq, sizeof(uint64_t));
    if (rezRead < sizeof(uint64_t)){
        procError("read minutes");
        exit_value = EXIT_FAILURE;
        goto lExit;
    }
    taskIdRq = be64toh(taskIdRq);

    //fetch task with id
    struct element_t *e;
    struct stTask *task;
    for (e=context->tasks->first; e!=NULL; e=e->next) {
        task = (struct stTask *) (e->data);

        //Task found
        if (task->taskId==taskIdRq){

            //Header
            int acc=sizeof(uint16_t)+ sizeof(uint32_t);
            char *buf= malloc(acc);
            char *tmp= NULL;
            if (buf==NULL){
                procError("Fail malloc");
                exit_value=EXIT_FAILURE;
                goto lExit;
            }
            uint16_t Retype = htobe16(SERVER_REPLY_OK);
            uint32_t nbRuns = htobe32(task->runs->count);
            memmove(buf, &Retype, sizeof(uint16_t));
            memmove(buf+ sizeof(uint16_t), &nbRuns, sizeof(uint32_t));

            //Runs
            struct stRunStat *s;
            for (e=task->runs->first; e; e=e->next){
                s=(struct stRunStat *) (e->data);

                //Time
                uint64_t time = htobe64(mktime(&s->stTime));

                //Code
                //https://man7.org/linux/man-pages/man2/wait.2.html                
                // WIFEXITED(wstatus)
                //        returns true if the child terminated normally, that is, by
                //        calling exit(3) or _exit(2), or by returning from main().
                // 
                // WEXITSTATUS(wstatus)
                //        returns the exit status of the child.  This consists of
                //        the least significant 8 bits of the status argument that
                //        the child specified in a call to exit(3) or _exit(2) or as
                //        the argument for a return statement in main().  This macro
                //        should be employed only if WIFEXITED returned true.                
                //We are using those 2 macro to check - was process closed normally (not by signal)
                //and if YES - return code 8 bits.
                uint16_t code = htobe16(0xFFFF); //no sense to do htobe16 for 0xFFFF but to keep consistency we are using it
                if (WIFEXITED(s->code)){
                    code = htobe16(WEXITSTATUS(s->code));
                }

                acc+= sizeof(uint16_t)+ sizeof(uint64_t);
                tmp=realloc(buf, acc);
                if (tmp){
                    buf = tmp;
                } else {
                    free(buf);
                    procError("Fail realloc");
                    exit_value=EXIT_FAILURE;
                    goto lExit;
                }

                int currDep = acc-(sizeof(uint64_t)+sizeof(uint16_t));
                memmove(buf+ currDep, &time, sizeof(uint64_t));
                memmove(buf+ currDep+ sizeof(uint64_t), &code, sizeof(uint16_t));
            }

            //Response
            writeReply(context, (uint8_t *)buf, acc);
            free(buf);

            goto lExit;
        }

    }

    //NotFound
    char buf[sizeof(uint16_t)+ sizeof(uint16_t)];
    uint16_t Retype = SERVER_REPLY_ERROR;
    uint16_t Errcode = SERVER_REPLY_ERROR_NOT_FOUND;
    memmove(buf, &Retype, sizeof(uint16_t));
    memmove(buf+ sizeof(uint16_t), &Errcode, sizeof(uint16_t));

    //Response
    writeReply(context, (uint8_t *)buf, sizeof(uint16_t)+ sizeof(uint16_t));

    lExit:
        return exit_value;
}

int processTerminate(struct stContext *context)
{
    uint16_t rep = htobe16(SERVER_REPLY_OK);
    context->exit = 1;
    return writeReply(context, (uint8_t*)&rep, sizeof(rep));
}

int processStdOutCmd(struct stContext *context)
{
    return sendFileContent(context, TASK_STD_OUT_NAME);
}

int processStdErrCmd(struct stContext *context)
{
    return sendFileContent(context, TASK_STD_ERR_NAME);
}


int maintainTasks(struct stContext *context){
    int               ret       = EXIT_SUCCESS;
    time_t            curTime   = time(NULL);
    struct tm         stCurTime = *localtime(&curTime);
    struct element_t *taskEl    = context->tasks->first;

    //while is element in list && has not error
    while ((taskEl) && (EXIT_SUCCESS == ret))
    {
        struct stTask * task = (struct stTask *)(taskEl->data);

        time_t taskLastExecTime = mktime(&task->stExecuted);

        if ((task->lastPid > 0) && (difftime(curTime, taskLastExecTime) >= 1.0)) //1 second later           
        {
            int status = 0;
            pid_t pidR = waitpid(task->lastPid, &status, WNOHANG);
            if (pidR == -1){
                procError("wait error!");
                ret = EXIT_FAILURE;
            } else if (pidR == 0){
                //still rinning!
            } else {
                struct stRunStat* run = (struct stRunStat*)malloc(sizeof(struct stRunStat));
                if (run)
                {
                    memset(run, 0, sizeof(struct stRunStat));
                    run->stTime = task->stExecuted;
                    run->code = status;
                    pushLast(task->runs, run);
                }

                task->lastPid = 0;
                CLOSE_FILE(task->stdOut);
                CLOSE_FILE(task->stdErr);
            }
        }

        //We have min granularity of 1 minute.
        //if last task is terminated && since last execution more than 60 seconds passed. 
        //&& task is programmed to be executed in current time
        if ((0 == task->lastPid)
             && (difftime(curTime, taskLastExecTime) >= 60.0)
             && (task->daysOfWeek[stCurTime.tm_wday])
             && (task->hours[stCurTime.tm_hour])
             && (task->minutes[stCurTime.tm_min])){
               ret = execTask(context, task);
               if ( EXIT_SUCCESS == ret){
               if (!IS_DAEMON){
                    printf("%02u:%02u:%02u Execute task %lu, {%s}\n", 
                    stCurTime.tm_hour,
                    stCurTime.tm_min,
                    stCurTime.tm_sec,
                    task->taskId, task->argV[0]->text);
                }     
                    task->stExecuted = stCurTime;    
               }
        }
        //check next task
        taskEl = taskEl->next;
    }

    return ret;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                               auxillary functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct stString *createStringBuffer(size_t len){
    struct stString *ret = (struct stString *)malloc(sizeof(struct stString));
    if (!ret) {
        return NULL;
    }

    ret->text = (char*)malloc(len + 1);
    if (ret->text){
        ret->len = len;
    } else {
        free(ret);
        ret = NULL;
    }

    return ret;
}


struct stString *createString(const char *str){
    if (!str){
        return NULL;
    }

    struct stString *ret = (struct stString *)malloc(sizeof(struct stString));
    if (!ret){
        return NULL;
    }

    ret->text = strdup(str);
    if (ret->text){
        ret->len = strlen(str);    
    } else {
        free(ret);
        ret = NULL;
    }

    return ret;
}

int freeString(struct stString *string)
{
    if (!string)
    {
        return -1;
    }

    if (string->text)
    {
        free(string->text);
        string->text = NULL;
    }

    free(string);

    return 0;
}

int freeTask(struct stTask *task){
    if (!task){
        return -1;
    }

    for(size_t i = 0; i < task->argC; i++){
        freeString(task->argV[i]);
    }
    
    free(task->argV);

    while(task->runs->first){
        free(task->runs->first->data);
        removeEl(task->runs, task->runs->first);
    }
    free(task->runs);

    free(task);
    return 0;
}


struct stString *createFilePath(const char *postfix){
    if (!postfix){
        return NULL;
    }               
                     
    char pPath[4096];
    GET_DEFAULT_PATH(pPath, postfix);

    struct stString *ret = createString(pPath);

    if (!ret){
        return NULL;
    }

    char *l_pIter = ret->text;

    while (*l_pIter){
        if ('/' == (*l_pIter)){
            *l_pIter = 0;

            if (    (!isDirExists(ret->text))
                 && (l_pIter != ret->text)
               ){
                mkdir(ret->text, S_IRWXU | S_IRWXG | S_IROTH);
            }
            *l_pIter = '/';
        }
        l_pIter++;
    }

    return ret;
}


int isDirExists(const char *path)
{
    struct stat l_sStat;
    l_sStat.st_mode = 0;
    if (0 == stat(path, &l_sStat))
    {
        if ((l_sStat.st_mode & S_IFDIR) != 0)
        {
            return 1;
        }
    }   
    return 0;
}

int isFileExists(const char *path)
{
    struct stat l_sStat;
    l_sStat.st_mode = 0;
    if (0 == stat(path, &l_sStat))
    {
        if ((0 != (l_sStat.st_mode & S_IFREG)) || (0 != (l_sStat.st_mode & S_IFIFO)))
        {
            return 1;
        }
    }
    return 0;
}


int writeReply(struct stContext *context, const uint8_t *buff, size_t size)
{
    int ret = EXIT_SUCCESS;
    if (!context || !buff)
    {
        return EXIT_FAILURE;
    }

    context->pipeReply = open(context->pipeRepName->text, O_WRONLY);   
    if (context->pipeReply < 0)
    {
        procError("open pipe is failed");
        ret = EXIT_FAILURE;
        goto lExit;
    }

    if (write(context->pipeReply, buff, size) < (ssize_t)size) 
    {
        procError("write to pipe failure");
        ret = EXIT_FAILURE;
        goto lExit;
    }

lExit:
    CLOSE_FILE(context->pipeReply);
    return ret;
}


int execTask(struct stContext *context, struct stTask * task)
{
    int ret = EXIT_SUCCESS;
    struct stString *fileOut = NULL;
    struct stString *fileErr = NULL;
    char txtBuf[1024];

    char ** argv = (char **)malloc(sizeof(char *)*(task->argC+1));
    if (!argv) {
        procError("read arg text");
        ret = EXIT_FAILURE;
        goto lExit;
    }
    
    for (size_t i = 0; i < task->argC; i++){
        argv[i] = task->argV[i]->text;
    }
    argv[task->argC] = NULL;//for execvp

    sprintf(txtBuf, "/tasks/%lu/%s", task->taskId, TASK_STD_OUT_NAME);
    fileOut = createFilePath(txtBuf);
    sprintf(txtBuf, "/tasks/%lu/%s", task->taskId, TASK_STD_ERR_NAME);
    fileErr = createFilePath(txtBuf);

    task->stdOut = open(fileOut->text, 
                        O_CREAT | O_RDWR | O_TRUNC,
                        S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH);
    task->stdErr = open(fileErr->text, 
                        O_CREAT | O_RDWR | O_TRUNC, 
                        S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH);

    if ((task->stdOut < 0) || (task->stdErr < 0)){
        procError("file open failed");
        ret = EXIT_FAILURE;
        goto lExit;
    }

    task->lastPid = fork();
    if (-1 == task->lastPid){
        procError("Fork failed");
        ret = EXIT_FAILURE;
        goto lExit;
    } 
    else if (0 == task->lastPid){
        //children does not use the parent's pipes 
        CLOSE_FILE(context->pipeReply);
        CLOSE_FILE(context->pipeRequest);

        //redirect stdin && stdout
        dup2(task->stdOut, STDOUT_FILENO);
        dup2(task->stdErr, STDERR_FILENO);

        //printf("%s %s\n", argv[0], argv[1]);

        execvp(task->argV[0]->text, argv);
        exit(0);
    } 

lExit:
    FREE_STR(fileOut);
    FREE_STR(fileErr);
    FREE_MEM(argv);

    if (ret != EXIT_SUCCESS)
    {
        CLOSE_FILE(task->stdOut);
        CLOSE_FILE(task->stdErr);
    }

    return ret;
}


int sendFileContent(struct stContext *context, const char *fileName)
{
    int               ret     = EXIT_SUCCESS;
    uint64_t          taskId  = 0;
    ssize_t           rezRead = 0;
    struct element_t *taskEl  = context->tasks->first;
    struct stTask    *task    = NULL;
    const size_t      replySize = 4096;
    uint8_t           replyBuf[replySize];
    int               fileFD   = -1;
    uint32_t          bufSize   = 0;
    uint32_t          fileSize  = 0;
    char              txtBuf[256];
    char              filePath[4096];
    struct pollfd     fds[1];

    // read task ID
    rezRead = read(context->pipeRequest, &taskId, sizeof(taskId));
    if (rezRead < sizeof(taskId)){
        procError("read task ID");
        ret = EXIT_FAILURE;
        goto lExit;
    }
    taskId = be64toh(taskId);

    context->pipeReply = open(context->pipeRepName->text, O_WRONLY);   
    if (context->pipeReply < 0){
        procError("open pipe is failed");
        ret = EXIT_FAILURE;
        goto lExit;
    }

    //find required task 
    while ((taskEl) && (EXIT_SUCCESS == ret)){
        task = (struct stTask *)(taskEl->data);
        if (task->taskId == taskId){
            break;
        }
        taskEl = taskEl->next;
    }

    //if task not found
    if (!taskEl){
        // Pattern of answer  REPTYPE='ER' <uint16>, ERRCODE <uint16>
        *(uint16_t*)replyBuf = htobe16(SERVER_REPLY_ERROR);
        *(uint16_t*)(replyBuf + sizeof(uint16_t)) = htobe16(SERVER_REPLY_ERROR_NOT_FOUND);
        bufSize = 2 * sizeof(uint16_t);

        if (write(context->pipeReply, replyBuf, bufSize) < (ssize_t)bufSize){
            procError("write to pipe failure");
            ret = EXIT_FAILURE;
        }
        goto lExit;
    }

    // create absolut path
    sprintf(txtBuf, "/tasks/%lu/%s", task->taskId, fileName);
    GET_DEFAULT_PATH(filePath, txtBuf);

    // open file
    fileFD = open(filePath, O_RDONLY);
    if (fileFD < 0){
        // Pattern of answer  REPTYPE='ER' <uint16>, ERRCODE <uint16>
        *(uint16_t*)replyBuf = htobe16(SERVER_REPLY_ERROR);
        *(uint16_t*)(replyBuf + sizeof(uint16_t)) = htobe16(SERVER_REPLY_ERROR_NEVER_RUN);
        bufSize = sizeof(uint16_t) + sizeof(uint16_t);

        if (write(context->pipeReply, replyBuf, bufSize) < (ssize_t)bufSize){
            procError("write to pipe failure");
            ret = EXIT_FAILURE;
        }
        goto lExit;
    }

    // Pattern of answer REPTYPE='OK' <uint16>, OUTPUT <string>   
    //find size of file & jump to beginning
    fileSize = lseek(fileFD, 0, SEEK_END);   
    lseek(fileFD, 0, SEEK_SET);

    *(uint16_t*)replyBuf = htobe16(SERVER_REPLY_OK);
    *(uint32_t*)(replyBuf + sizeof(uint16_t)) = htobe32((uint32_t)fileSize);
    bufSize = sizeof(uint16_t) + sizeof(uint32_t);
    if (write(context->pipeReply, replyBuf, bufSize) < (ssize_t)bufSize){
        procError("write to pipe failure");
        ret = EXIT_FAILURE;
    }

    fds[0].fd      = context->pipeReply;
    fds[0].events  = POLLOUT;
    fds[0].revents = 0;    

    while (fileSize)
    {
        int pollRet = poll(fds, 1, 5000); //5 seconds to wait 
        if (pollRet > 0)
        {
            bufSize = (((uint32_t)replySize < fileSize) ? (uint32_t)replySize : fileSize);
            if (bufSize != read(fileFD, replyBuf, bufSize)){
                procError("file read error!");
                ret = EXIT_FAILURE;
                break;
            }

            if (write(context->pipeReply, replyBuf, bufSize) < (ssize_t)bufSize){
                procError("write to pipe failure");
                ret = EXIT_FAILURE;
            }

            fileSize -= bufSize;
        }
        else{
            procError("Pipe write timeout or error");
            ret = EXIT_FAILURE;
            break;
        }
    }

lExit:
    CLOSE_FILE(context->pipeReply);
    CLOSE_FILE(fileFD);

    return ret;
}

int procError(const char *msg){
    if (IS_DAEMON){
        syslog (LOG_ERR, "%s", msg);
    } else {
        perror(msg);
    }
}
int procInfo(const char *msg){
    if (IS_DAEMON){
        syslog (LOG_NOTICE, "%s", msg);
    } else {
        printf("%s\n", msg);
    }
}

int closeDeamonLog(){
    if (IS_DAEMON){
        closelog();
    }
}

