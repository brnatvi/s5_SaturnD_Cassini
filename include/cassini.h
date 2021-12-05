#ifndef CASSINI_H
#define CASSINI_H

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

#include "client-request.h"
#include "server-reply.h"

#define CLOSE_FILE(File) if (File > 0) {close(File); File = -1;}
#define FREE_MEM(Mem) if (Mem) {free(Mem); Mem = NULL;}


int isBigEndian();
char * create_path(char *pipes_directory, int isRequets);
int create_task(int request, int reply, char *minutes_str, char *hours_str, char *daysofweek_str, int argc, char *argv[]);
int list_task(int request, int reply);
int remove_task(int request, int reply, uint64_t taskid);
int rq_stdout_stderr(int request, int reply, uint64_t taskid,uint16_t operation, int isBigE);
int terminate(int request, int reply, int isBigE);
int times_exitcodes(int request, int reply, uint64_t taskid);

#endif // CASSINI
