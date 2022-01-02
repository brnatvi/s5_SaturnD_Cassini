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
#include <time.h>
#include <timing-text-io.h>
#include <cassini.h>
#include <sys/stat.h>
#include <timing.h>
#include <poll.h>

#include "listd.h"

#include "client-request.h"
#include "server-reply.h"
#include "pipes-path.h"
#include "common-utils.h"

struct stString
{
    char  *text;
    size_t len;
};

struct stTask
{
    uint64_t          taskId;
    unsigned char     minutes[60];    //array representing all minutes of 1h, for example [0, 0, 1, 1, 0, 0 ... ] => minutes 3,4 are active, all others are inactive
    unsigned char     hours[24];      //array representing all hours of a day, for example [0, 1, 0, 1, 0, 0 ... ] => hours 2,4 are active, all others are inactive
    unsigned char     daysOfWeek[7];  //array representing all days of a week, for example [1, 1, 0, 1, 0, 0, 0] => Sunday, Monday, Wednesday are active, all others are inactive
    size_t            argC;
    struct stString **argV;     
    int               stdOut;
    int               stdErr;
    struct tm         stCreated;
    struct tm         stExecuted;
} ;

struct stContext
{
    struct listElements_t  *tasks;
    int                     pipeRequest;
    int                     pipeReply;
    uint64_t                lastTaskId; 
    int                     exit;
};


int restoreTasksFromHdd(struct stContext *context);
int saveTasksToHdd(struct stContext *context);
int processListCmd(struct stContext *context);
int processCreateCmd(struct stContext *context);
int processRemoveCmd(struct stContext *context);
int processTimesExitCodesCmd(struct stContext *context);
int processStdOutCmd(struct stContext *context);
int processStdErrCmd(struct stContext *context);
int maintainTasks(struct stContext *context);

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

#endif //SATURND_H