#include "saturnd.h"

int main(int argc, char *argv[]) 
{
    int              ret = EXIT_SUCCESS;
    struct pollfd    fds[1];
    int              cycle = 0;

    // 1) Create context stContext 
    struct stContext *context = (struct stContext*) malloc(sizeof(struct stContext));
    if (!context) 
    {
        perror("malloc context is failed");
        ret = EXIT_FAILURE;
        goto lExit;
    }    
    memset(context, 0, sizeof(struct stContext));

    context->tasks = (struct listElements_t*) malloc(sizeof(struct listElements_t));
    if (!context->tasks) {
        perror("malloc context->tasks is failed");
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
            perror("make fifo is failure");
            ret = EXIT_FAILURE;
            goto lExit;
        }
    }

    if (!isFileExists(context->pipeRepName->text)){
        if (mkfifo(context->pipeRepName->text, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) < 0){
            perror("make fifo is failure");
            ret = EXIT_FAILURE;
            goto lExit;
        }
    }

    //obtain fd for reply-pipe and request-pipe, using NONBLOCK more for request FIFO to be capable to open it 
    //even if on other side there is no any process

    //WARNING: pipe is opened with O_RDWR instead O_RDONLY for next reason:
    //function **poll** returns with POLLHUP value in fds[0].revents constantly and immediately after first 
    //communication session with Cassini -> cassini close the request pipe and this action force Saturn to
    //return from **poll** all the time with POLLHUP return code.
    //this behavious produce serions CPU load, and to optimize it next hack was used:
    //https://stackoverflow.com/questions/22021253/poll-on-named-pipe-returns-with-pollhup-constantly-and-immediately
    //N.B.: way how pipes are working under POSIX systems isn't looking user-friendly unfortunatelly.
    context->pipeRequest = open(context->pipeReqName->text, O_RDWR | O_NONBLOCK);   
    if (context->pipeRequest < 0){
        perror("open fifo is failed");
        ret = EXIT_FAILURE;
        goto lExit;
    }

    // 3) Load from disc list of the tasks and store it in context->pTasks
    ret = restoreTasksFromHdd(context);
    if (EXIT_SUCCESS != ret)
    {
        goto lExit;
    }

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
                        perror("unknown request!");
                        ret = EXIT_FAILURE;
                        break;
                }
            }

        } else if (pollRet < 0){ 
            perror("poll error");
            ret = EXIT_FAILURE;
            break;
        }

        if (EXIT_SUCCESS == ret){
            ret = maintainTasks(context);
        }
      //  cycle++;
      //  if (cycle % 10)
      //  {
      //      printf(".");
      //  }
      //  else
      //  {
      //      printf(".\n");
      //  }
    }

    if (EXIT_SUCCESS == ret){
        ret = saveTasksToHdd(context);
    }

lExit:
    //clear all list elements
    while(context->tasks->first){
        printf("delete task %lu\n", ((struct stTask *)context->tasks->first->data)->taskId);
        freeTask((struct stTask *)context->tasks->first->data);
        removeEl(context->tasks, context->tasks->first);
    }

    FREE_MEM(context->tasks); 
    CLOSE_FILE(context->pipeReply);
    CLOSE_FILE(context->pipeRequest);  
    FREE_STR(context->pipeReqName);
    FREE_STR(context->pipeRepName);
    FREE_MEM(context);

    return ret;
}



// Restore content of context->tasks from disk
int restoreTasksFromHdd(struct stContext *context){    
    return EXIT_SUCCESS;
}

// Save content of context->tasks to disk
int saveTasksToHdd(struct stContext *context){
    return EXIT_SUCCESS;
}

int processListCmd(struct stContext *context){
    return EXIT_SUCCESS;
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
        perror("memory allocation");
        retCode = EXIT_FAILURE;
        goto lExit;
    }

    newTask->runs = (struct listElements_t *)malloc(sizeof(struct listElements_t));
    if (!newTask->runs){
        perror("memory allocation");
        retCode = EXIT_FAILURE;
        goto lExit;
    }
    memset(newTask->runs, 0, sizeof(struct listElements_t));

    // read timing
    rezRead = read(context->pipeRequest, &min, sizeof(min));
    if (rezRead < sizeof(min)){
        perror("read minutes");
        retCode = EXIT_FAILURE;
        goto lExit;
    }
    min = be64toh(min);

    rezRead = read(context->pipeRequest, &hours, sizeof(hours));
    if (rezRead < sizeof(hours)){
        perror("read hours");
        retCode = EXIT_FAILURE;
        goto lExit;
    }
    hours = be32toh(hours);

    rezRead = read(context->pipeRequest, &days, sizeof(days));
    if (rezRead < sizeof(days)){
        perror("read daysOfWeek");
        retCode = EXIT_FAILURE;
        goto lExit;
    }

    // fill newTask daysOfWeek, hours, minutes 
    for (int i = 0; i < 7; i++){
        if (days & (1 << i)) {
            newTask->daysOfWeek[i] = 1;
        } else {
            newTask->daysOfWeek[i] = 0;
        }
    }

    for (int i = 0; i < 24; i++){
        if (hours & (1 << i)){
            newTask->hours[i] = 1;
        } else {
            newTask->hours[i] = 0;
        }
    }

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
        perror("read argc");
        retCode = EXIT_FAILURE;
        goto lExit;
    }
    argc = be32toh(argc);
    newTask->argC = (size_t)argc;

    newTask->argV = (struct stString **)malloc(sizeof(struct stString *) * newTask->argC);
    if (!newTask->argV){
        perror("memory allocation");
        retCode = EXIT_FAILURE;
        goto lExit;
    }

    for (size_t i = 0; i < newTask->argC; i++) //read all command's arguments
    {
        int strLen = 0;
        rezRead = read(context->pipeRequest, &strLen, sizeof(strLen));
        if (rezRead < sizeof(strLen))
        {
            perror("read argc");
            retCode = EXIT_FAILURE;
            break;
        }
        strLen = be32toh(strLen); 
        newTask->argV[i] = createStringBuffer(strLen);

        rezRead = read(context->pipeRequest, newTask->argV[i]->text, newTask->argV[i]->len);
        if (rezRead < newTask->argV[i]->len)
        {
            perror("read arg text");
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
        perror("read task ID");
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
        sprintf(txtBuf, "/tasks/%lu/stdout", task->taskId);
        GET_DEFAULT_PATH(txtPath, txtBuf);
        remove(txtPath);

        sprintf(txtBuf, "/tasks/%lu/stderr", task->taskId);
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
    int ret = EXIT_SUCCESS;


    return ret;
}

int processTerminate(struct stContext *context)
{
    uint16_t rep = htobe16(SERVER_REPLY_OK);
    context->exit = 1;
    return writeReply(context, (uint8_t*)&rep, sizeof(rep));
}

int processStdOutCmd(struct stContext *context)
{
    return EXIT_SUCCESS;
}

int processStdErrCmd(struct stContext *context)
{
    return EXIT_SUCCESS;
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

        if (    (0 < task->lastPid)
             && (5.0 <= difftime(curTime, taskLastExecTime)) //5 seconds later
           )
        {
            int status = 0;
            pid_t pidR = waitpid(task->lastPid, &status, WNOHANG);
            if (pidR == -1){
                perror("wait error!");
                ret = EXIT_FAILURE;
            } else if (pidR == 0){
                //still rinning!
            } else {
                struct stRunStat* run = (struct stRunStat*)malloc(sizeof(struct stRunStat));
                if (run)
                {
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
             && (60.0 <= difftime(curTime, taskLastExecTime))
             && (task->daysOfWeek[stCurTime.tm_wday])
             && (task->hours[stCurTime.tm_hour])
             && (task->minutes[stCurTime.tm_min])){
               ret = execTask(context, task);
               if ( EXIT_SUCCESS == ret)
               {
                    printf("%02u:%02u:%02u Execute task %lu, {%s}\n", 
                    stCurTime.tm_hour,
                    stCurTime.tm_min,
                    stCurTime.tm_sec,
                    task->taskId, task->argV[0]->text);

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
        perror("open fifo is failed");
        ret = EXIT_FAILURE;
        goto lExit;
    }

    if (write(context->pipeReply, buff, size) < (ssize_t)size) 
    {
        perror("write to pipe failure");
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
        perror("read arg text");
        ret = EXIT_FAILURE;
        goto lExit;
    }
    
    for (size_t i = 0; i < task->argC; i++){
        argv[i] = task->argV[i]->text;
    }
    argv[task->argC] = NULL;//for execvp

    sprintf(txtBuf, "/tasks/%lu/stdout", task->taskId);
    fileOut = createFilePath(txtBuf);
    sprintf(txtBuf, "/tasks/%lu/stderr", task->taskId);
    fileErr = createFilePath(txtBuf);

    task->stdOut = open(fileOut->text, 
                        O_CREAT | O_RDWR | O_TRUNC,
                        S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH);
    task->stdErr = open(fileErr->text, 
                        O_CREAT | O_RDWR | O_TRUNC, 
                        S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH);

    if ((task->stdOut < 0) || (task->stdErr < 0)){
        perror("file open failed");
        ret = EXIT_FAILURE;
        goto lExit;
    }

    task->lastPid = fork();
    if (-1 == task->lastPid){
        perror("Fork failed");
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
