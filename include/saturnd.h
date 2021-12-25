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
#include <timing.h>
#include "listd.h"

#include "client-request.h"
#include "server-reply.h"

#define CLOSE_FILE(File) if (File > 0) {close(File); File = -1;}
#define FREE_MEM(Mem) if (Mem) {free(Mem); Mem = NULL;}

struct stString
{
    char  *text;
    size_t len;
};

struct stTask
{
    int              taskId;
    unsigned char    minutes[60];
    unsigned char    hours[24];
    unsigned char    daysOfWeek[7];    
    struct stString  stCommand;     
    size_t           argC;
    struct stString *argV;     
    int              stdOut;
    int              stdErr;
    unsigned int     repeatable;
    struct tm        stCreated;
    struct tm        stExecuted;
};

struct stContext
{
    struct listElements_t  *tasks;
    int                     pipeRequest;
    int                     pipeReply;
    //TODO: other fields ?
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


#endif //SATURND_H
