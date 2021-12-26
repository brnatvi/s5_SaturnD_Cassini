#include "saturnd.h"

int main(int argc, char *argv[]) 
{
    unsigned int ret = EXIT_SUCCESS;
    int opt;
    char *pipes_directory = NULL;  
    char *pipe_dir_req = NULL;
    char *pipe_dir_rep = NULL;   

    while ((opt = getopt(argc, argv, "hlcqm:H:d:p:r:x:o:e:")) != -1)   //TODO refactoring according options to Saturnd
    {
        switch (opt) 
        {            
            case 'p':
                pipes_directory = strdup(optarg);                
                break;
        }
    }

    // 1) Create context stContext 
    struct stContext *context = (struct stContext*) malloc(sizeof(struct stContext));
    if (!context)
    {
        perror("malloc context is failed");
        ret = EXIT_FAILURE;
        goto error;
    }
    memset(context, 0, sizeof(struct stContext));

    context->tasks = (struct listElements_t*) malloc(sizeof(struct listElements_t));
    if (!context->tasks)
    {
        perror("malloc context->tasks is failed");
        ret = EXIT_FAILURE;
        goto error;
    }
    memset(context->tasks, 0, sizeof(struct listElements_t));


    // 2) if not existing: creates 2 pipes, else: open
    // default  /tmp/<USER_NAME>/saturnd/pipes or -p <PIPES_DIR>

    // create path 
    pipe_dir_req = create_path(pipes_directory, 0);
    pipe_dir_rep = create_path(pipes_directory, 1);

    if ((!pipe_dir_req) || (!pipe_dir_rep)) {
        perror("can't allocate memory for directories path");
        ret = EXIT_FAILURE;
        goto error;
    }
    
    // obtain fd for reply-pipe and request-pipe
    context->pipeRequest = open(pipe_dir_req, O_RDONLY);        // open return -1 or fd
    context->pipeReply   = open(pipe_dir_rep, O_WRONLY);

    if ((context->pipeRequest < 0) || (context->pipeReply < 0))
    {
        // create pipes
        int rezMakeFifoRequest = mkfifo(pipe_dir_req, S_IRUSR | S_IRGRP | S_IROTH);
        int rezMakeFifoReply   = mkfifo(pipe_dir_rep, S_IWUSR | S_IWGRP | S_IWOTH);  

        if ((rezMakeFifoRequest < 0) || (rezMakeFifoReply < 0)) 
        {
            perror("make fifo is failure");
            ret = EXIT_FAILURE;
            goto error;
        }  

        context->pipeRequest = open(pipe_dir_req, O_RDONLY);        // open return -1 or fd
        context->pipeReply   = open(pipe_dir_rep, O_WRONLY);
        if ((context->pipeRequest < 0) || (context->pipeReply < 0))
        {
            perror("open request-pipe failure");
            ret = EXIT_FAILURE;
            goto error;
        }
    }   
 

    // 3) Load from disc list of the tasks and store it in context->pTasks

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

    //TODO: clear internal context field, like list, close pipes, etc.
error:
    if (errno != 0) {
        perror("main");
    }

    //clear all list elements
    while(context->tasks->first)
    {
        freeTask((struct stTask *)context->tasks->first->data);
        removeEl(context->tasks, context->tasks->first);
    }
    FREE_MEM(context->tasks); 
    FREE_MEM(context);
    FREE_MEM(pipes_directory);
    FREE_MEM(pipe_dir_req);
    FREE_MEM(pipe_dir_rep);
    CLOSE_FILE(context->pipeReply);
    CLOSE_FILE(context->pipeRequest);   

    return ret;
}



// Restore content of context->tasks from disk
int restoreTasksFromHdd(struct stContext *context)
{
    return 0;
}

// Save content of context->tasks to disk
int saveTasksToHdd(struct stContext *context)
{
    return 0;
}

int processListCmd(struct stContext *context)
{
    return 0;
}

int processCreateCmd(struct stContext *context)
{
    return 0;
}

int processRemoveCmd(struct stContext *context)
{
    return 0;
}

int processTimesExitCodesCmd(struct stContext *context)
{
    return 0;
}

int processStdOutCmd(struct stContext *context)
{
    return 0;
}

int processStdErrCmd(struct stContext *context)
{
    return 0;
}

int maintainTasks(struct stContext *context)
{
    return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                               auxillary functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct stString *createString(const char *str, size_t len)
{
    if (!str)
    {
        return NULL;
    }

    struct stString *ret = (struct stString *)malloc(sizeof(struct stString));
    if (!ret)
    {
        return NULL;
    }

    if (len)
    {
        ret->text = (char*)malloc(len+1);
        if (ret->text)
        {
            memcpy(ret->text, str, len);
            ret->text[len] = 0;
            ret->len = len;    
        }
        else
        {
            free(ret);
            ret = NULL;
        }
    }
    else
    {
        ret->text = strdup(str);
        if (ret->text)
        {
            ret->len = len;    
        }
        else
        {
            free(ret);
            ret = NULL;
        }
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

int freeTask(struct stTask *task)
{
    if (!task)
    {
        return -1;
    }

    freeString(task->command);
    for(size_t i = 0; i < task->argC; i++)
    {
        freeString(task->argV[i]);
    }
    free(task->argV);

    free(task);
    return 0;
}



