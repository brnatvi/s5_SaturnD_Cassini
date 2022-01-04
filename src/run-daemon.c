#include "run-daemon.h"

void createChild(){
    switch (fork()) {
        case 0:
            break;
        case -1:
            exit(EXIT_FAILURE);
        default:
            exit(EXIT_SUCCESS);
    }
}

void run_daemon(){

        // Allows keeping only the child process.
        createChild();

        // Creation of a new session for the child process.
        if (setsid() < 0) exit(EXIT_FAILURE);

        // here: possible to implement a signal handler

        // The double fork avoids having to wait for the child when no synchronization is needed.
        createChild();

        // Close all open file descriptors that can be inherited from the parent.
        for (int x = sysconf(_SC_OPEN_MAX); x>=0; x--)close (x);

        // Allows you to update the log system.
        openlog ("saturnd", LOG_PID, LOG_DAEMON);

}