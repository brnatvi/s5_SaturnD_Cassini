#include "saturnd.h"

int main(int argc, char *argv[]) 
{
    struct stString *pipeReqName = NULL;
    struct stString *pipeRepName = NULL;
    int              ret         = EXIT_SUCCESS;
    struct pollfd    fds[1];

    // 1) Create context stContext 
    struct stContext *context = (struct stContext*) malloc(sizeof(struct stContext));
    if (!context) {
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

    // 2) if not existing: creates 2 pipes, else: open
    // default  /tmp/<USER_NAME>/saturnd/pipes or -p <PIPES_DIR>
    // create path 
    pipeReqName = createFilePath("pipes" PIPE_REQUEST_NAME);
    pipeRepName = createFilePath("pipes" PIPE_REPLY_NAME);

    printf("{%s} {%s}\n", pipeReqName->text, pipeRepName->text);
    printf("DBG:3\n");

    if (!isFileExists(pipeReqName->text)){
        if (mkfifo(pipeReqName->text, S_IRUSR | S_IRGRP | S_IROTH) < 0){
            perror("make fifo is failure");
            ret = EXIT_FAILURE;
            goto lExit;
        }
    }

    printf("DBG:4\n");
    if (!isFileExists(pipeRepName->text)){
        if (mkfifo(pipeRepName->text, S_IRUSR | S_IRGRP | S_IROTH) < 0){
            perror("make fifo is failure");
            ret = EXIT_FAILURE;
            goto lExit;
        }
    }

    printf("DBG:5\n");
    // obtain fd for reply-pipe and request-pipe, using NONBLOCK more for request FIFO to be capable to open it 
    //even if on other side there is no any process
    context->pipeRequest = open(pipeReqName->text, O_RDONLY | O_NONBLOCK);        // open return -1 or fd
    //context->pipeReply   = open(pipe_dir_rep, O_WRONLY | O_NONBLOCK);
    printf("DBG:6\n");

    if (context->pipeRequest < 0){
        perror("open fifo is failed");
        ret = EXIT_FAILURE;
        goto lExit;
    }

    // 3) Load from disc list of the tasks and store it in context->pTasks
    ret = restoreTasksFromHdd(context);
    if (EXIT_SUCCESS == ret)
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
    //                                    what if cassini doesn't sent all?????
    //          put the task to list
    //          execute the task by fork
    //          send the report to reply-pipe
    //        If temrinate = exit loop
    // - from time to time checks timings of tasks in context->pTasks - if need to execute some of them
    fds[0].fd      = context->pipeRequest;
    fds[0].events  = POLLIN;
    fds[0].revents = 0;    

    while (    (0 == context->exit)
            && (EXIT_SUCCESS == ret)
          ) 
    {
        int pollRet = poll(fds, 1, 500); //2 times per second
        if (0 < pollRet){
            uint16_t code = 0x0;
            size_t szRead = read(context->pipeRequest, &code, sizeof(code));
            if (szRead < sizeof(code))
            {
                perror("read code error");
                ret = EXIT_FAILURE;
                break;
            }

            code = be16toh(code);

            switch (code)
            {
                case CLIENT_REQUEST_LIST_TASKS              : ret = processListCmd(context); break;
                case CLIENT_REQUEST_CREATE_TASK             : ret = processCreateCmd(context); break;
                case CLIENT_REQUEST_REMOVE_TASK             : ret = processRemoveCmd(context); break;
                case CLIENT_REQUEST_GET_TIMES_AND_EXITCODES : ret = processTimesExitCodesCmd(context); break;
                case CLIENT_REQUEST_TERMINATE               : /*TODO: shall we create special function?*/ break;
                case CLIENT_REQUEST_GET_STDOUT              : ret = processStdOutCmd(context); break;
                case CLIENT_REQUEST_GET_STDERR              : ret = processStdErrCmd(context); break;
                default :
                    perror("unknown request!");
                    ret = EXIT_FAILURE;
                    break;
            }

        } else if (0 > pollRet) { 
            perror("poll error");
            ret = EXIT_FAILURE;
            break;
        }

        if (EXIT_SUCCESS == ret)
        {
            ret = maintainTasks(context);
        }
    }

    if (EXIT_SUCCESS == ret)
    {
        ret = saveTasksToHdd(context);
    }

lExit:
    //clear all list elements
    while(context->tasks->first)
    {
        freeTask((struct stTask *)context->tasks->first->data);
        removeEl(context->tasks, context->tasks->first);
    }

    FREE_MEM(context->tasks); 

    CLOSE_FILE(context->pipeReply);
    CLOSE_FILE(context->pipeRequest);   

    FREE_MEM(context);

    FREE_STR(pipeReqName);
    FREE_STR(pipeRepName);
    return ret;
}



// Restore content of context->tasks from disk
int restoreTasksFromHdd(struct stContext *context)
{    
    return EXIT_SUCCESS;
}

// Save content of context->tasks to disk
int saveTasksToHdd(struct stContext *context)
{
    return EXIT_SUCCESS;
}

int processListCmd(struct stContext *context)
{
    return EXIT_SUCCESS;
}

int processCreateCmd(struct stContext *context)
{
    int            retCode = EXIT_SUCCESS;
    uint64_t       min     = 0;
    uint32_t       hours   = 0;
    uint8_t        days    = 0;
    int            argc    = 0;
    ssize_t        rezRead = 0;
    time_t         curTime = time(NULL);
    struct stTask *newTask = (struct stTask*) malloc(sizeof(struct stTask)); // create structure for new task

    if (!newTask)
    {
        perror("memory allocation");
        retCode = EXIT_FAILURE;
        goto lExit;
    }

    // read timing
    rezRead = read(context->pipeRequest, &min, sizeof(min));
    if (rezRead < sizeof(min))
    {
        perror("read minutes");
        retCode = EXIT_FAILURE;
        goto lExit;
    }
    min = be64toh(min);

    rezRead = read(context->pipeRequest, &hours, sizeof(hours));
    if (rezRead < sizeof(hours))
    {
        perror("read hours");
        retCode = EXIT_FAILURE;
        goto lExit;
    }
    hours = be64toh(hours);

    rezRead = read(context->pipeRequest, &days, sizeof(days));
    if (rezRead < sizeof(days))
    {
        perror("read daysOfWeek");
        retCode = EXIT_FAILURE;
        goto lExit;
    }
    days = be64toh(days);

    // fill newTask daysOfWeek, hours, minutes 
    for (int i = 0; i < 7; i++)
    {
        if (days & (1 << i))
        {
            newTask->daysOfWeek[i] = 1;
        }
        else
        {
            newTask->daysOfWeek[i] = 0;
        }
    }

    for (int i = 0; i < 24; i++)
    {
        if (hours & (1 << i))
        {
            newTask->hours[i] = 1;
        }
        else
        {
            newTask->hours[i] = 0;
        }
    }

    for (int i = 0; i < 60; i++)
    {
        if (min & (1 << i))
        {
            newTask->minutes[i] = 1;
        }
        else
        {
            newTask->minutes[i] = 0;
        }
    }

    // fill argC
    rezRead = read(context->pipeRequest, &argc, sizeof(argc));
    if (rezRead < sizeof(argc))
    {
        perror("read argc");
        retCode = EXIT_FAILURE;
        goto lExit;
    }
    argc = be64toh(argc);
    newTask->argC = (size_t)argc;
    newTask->argV = (struct stString **)malloc(sizeof(struct stString *) * newTask->argC);
    if (!newTask->argV)
    {
        perror("memory allocation");
        retCode = EXIT_FAILURE;
        goto lExit;
    }

    for (size_t i = 0; i < newTask->argC; i++) //real all command's arguments
    {
        int strLen = 0;
        rezRead = read(context->pipeRequest, &strLen, sizeof(strLen));
        if (rezRead < sizeof(strLen))
        {
            perror("read argc");
            retCode = EXIT_FAILURE;
            break;
        }
                                        
        newTask->argV[i] = createStringBuffer(be32toh(strLen));

        rezRead = read(context->pipeRequest, newTask->argV[i]->text, newTask->argV[i]->len);
        if (rezRead < newTask->argV[i]->len)
        {
            perror("read arg text");
            retCode = EXIT_FAILURE;
            break;
        }

        newTask->argV[i]->text[strLen] = 0;
    }

    if (retCode != EXIT_SUCCESS)
    {
        goto lExit;
    }

    newTask->taskId = ++context->lastTaskId;
    newTask->stCreated = *localtime(&curTime);

lExit:
    return retCode;
}

int processRemoveCmd(struct stContext *context)
{
    return EXIT_SUCCESS;
}

int processTimesExitCodesCmd(struct stContext *context)
{
    return EXIT_SUCCESS;
}

int processStdOutCmd(struct stContext *context)
{
    return EXIT_SUCCESS;
}

int processStdErrCmd(struct stContext *context)
{
    return EXIT_SUCCESS;
}
int maintainTasks(struct stContext *context)
{
    return EXIT_SUCCESS;
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


int isDirExists(const char *path){
    struct stat l_sStat;
    l_sStat.st_mode = 0;
    if (0 == stat(path, &l_sStat)){
        if (0 != (l_sStat.st_mode & S_IFDIR)){
            return 1;
        }
    }               

    return 0;
}

int isFileExists(const char *path){
    struct stat l_sStat;
    l_sStat.st_mode = 0;
    if (0 == stat(path, &l_sStat)){
        if (    (0 != (l_sStat.st_mode & S_IFREG))
             || (0 != (l_sStat.st_mode & S_IFIFO))
           ){
            return 1;
        }
    }

    return 0;
}
