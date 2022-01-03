#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H

#define CLOSE_FILE(File) if (File > 0) {close(File); File = -1;}
#define FREE_MEM(Mem) if (Mem) {free(Mem); Mem = NULL;}
#define FREE_STR(str) freeString(str); str = NULL;

#if !defined(STDIN_FILENO)
    #define	STDIN_FILENO	0	/* Standard input.  */
#endif 

#if !defined(STDOUT_FILENO)
    #define	STDOUT_FILENO	1	/* Standard output.  */
#endif 

#if !defined(STDERR_FILENO)
    #define	STDERR_FILENO	2	/* Standard error output.  */
#endif 


#endif //COMMON_UTILS_H
