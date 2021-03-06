#ifndef SATURND_H
#define SATURND_H

#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <endian.h>
#include <unistd.h>
#include <inttypes.h>
#include <time.h>
#include <timing-text-io.h>
#include <cassini.h>
#include <sys/stat.h>
#include <timing.h>
#include <poll.h>
#include <sys/wait.h>
#include <dirent.h>

#include "listd.h"

#include "client-request.h"
#include "server-reply.h"
#include "pipes-path.h"
#include "common-utils.h"
#include "run-daemon.h"

#define TASK_STD_OUT_NAME "stdout"
#define TASK_STD_ERR_NAME "stderr"

#define TASKS_FILE        "tasks.bin"

#define IS_DAEMON         1

struct stString
{
    char  *text;
    size_t len;
};

struct stRunStat
{
    struct tm  stTime;
    int        code;
};


struct stTask
{
    uint64_t               taskId;
    unsigned char          minutes[60];    //array representing all minutes of 1h, for example [0, 0, 1, 1, 0, 0 ... ] => minutes 3,4 are active, all others are inactive
    unsigned char          hours[24];      //array representing all hours of a day, for example [0, 1, 0, 1, 0, 0 ... ] => hours 2,4 are active, all others are inactive
    unsigned char          daysOfWeek[7];  //array representing all days of a week, for example [1, 1, 0, 1, 0, 0, 0] => Sunday, Monday, Wednesday are active, all others are inactive
    uint64_t               min;
    uint32_t               heu;
    uint8_t                day;
    size_t                 argC;
    struct stString      **argV;     
    int                    stdOut;
    int                    stdErr;
    struct tm              stCreated;
    struct tm              stExecuted;
    pid_t                  lastPid;        //pid of his last process
    struct listElements_t *runs;           //list of stRunStat
};

struct stContext
{
    struct listElements_t  *tasks; //list of stTask
    int                     pipeRequest;
    int                     pipeReply;
    uint64_t                lastTaskId; 
    int                     exit;
    struct stString        *pipeReqName;
    struct stString        *pipeRepName;
};


int restoreTasksFromHdd(struct stContext *context);
int saveTasksToHdd(struct stContext *context);
int processListCmd(struct stContext *context);          //done
int processCreateCmd(struct stContext *context);        //done
int processRemoveCmd(struct stContext *context);        //done
int processTimesExitCodesCmd(struct stContext *context);//done
int processTerminate(struct stContext *context);        //done
int processStdOutCmd(struct stContext *context);        //done
int processStdErrCmd(struct stContext *context);        //done 
int maintainTasks(struct stContext *context);           //done
int sendFileContent(struct stContext *context, const char *fileName);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                               auxillary functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct stString *createStringBuffer(size_t len);
struct stString *createString(const char *str);

int              freeString(struct stString *string);
int              freeTask(struct stTask *task);

struct stString *createFilePath(const char *postfix);

int              isDirExists(const char *path);
int              isFileExists(const char *path);
int              writeReply(struct stContext *context, const uint8_t *buff, size_t size);
int              execTask(struct stContext *context, struct stTask * task);

int              procError(const char *msg);
int              procInfo(const char *msg);
int              closeDeamonLog();


#endif //SATURND_H
