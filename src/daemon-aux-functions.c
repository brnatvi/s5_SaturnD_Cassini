#include "saturnd.h"


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