#include "saturnd.h"



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
