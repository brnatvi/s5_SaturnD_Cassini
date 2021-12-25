#include "saturnd.h"

int main(int argc, char *argv[]) 
{
    // Saturn:
    // 1) Create context stContext 
    struct stContext *context = (struct stContext*) malloc(sizeof(struct stContext));

    if (!context)
    {
        //todo: error
        exit(1);
    }

    context->tasks = (struct listElements_t*) malloc(sizeof(struct listElements_t));

    // 2) if not existing: creates 2 pipes, else: open
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
    free(context);
}

// Handlers:

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

