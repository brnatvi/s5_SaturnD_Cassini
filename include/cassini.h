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
#include "pipes-path.h"
#include "common-utils.h"

char * create_path(char *pipes_directory, int isRequets);
int create_task(char* request, char* reply, char *minutes_str, char *hours_str, char *daysofweek_str, int argc, char *argv[]);
int list_task(char* request, char* reply);
int remove_task(char* request, char* reply, uint64_t taskid);
int rq_stdout_stderr(char* request, char* reply, uint64_t taskid,uint16_t operation);
int terminate(char* request, char* reply);
int times_exitcodes(char* request, char* reply, uint64_t taskid);


#endif // CASSINI
