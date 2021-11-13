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

#include "client-request.h"
#include "server-reply.h"

#define CLOSE_FILE(File) if (File > 0) {close(File); File = -1;}
#define FREE_MEM(Mem) if (Mem) {free(Mem); Mem = NULL;}


int isBigEndian();
char * create_path(char *pipes_directory, int isRequets);
int create_task(int request, int reply, char *minutes_str, char *hours_str, char *daysofweek_str, int argc, char *argv[]);

#endif // CASSINI
