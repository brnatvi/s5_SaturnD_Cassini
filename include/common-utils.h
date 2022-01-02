#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H

#define CLOSE_FILE(File) if (File > 0) {close(File); File = -1;}
#define FREE_MEM(Mem) if (Mem) {free(Mem); Mem = NULL;}
#define FREE_STR(str) freeString(str); str = NULL;


#endif //COMMON_UTILS_H
