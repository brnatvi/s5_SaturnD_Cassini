#include "cassini.h"



int main(int argc, char *argv[]) {
    const char usage_info[] =
    "\
   usage: cassini [OPTIONS] -l -> list all tasks\n\
      or: cassini [OPTIONS]    -> same\n\
      or: cassini [OPTIONS] -q -> terminate the daemon\n\
      or: cassini [OPTIONS] -c [-m MINUTES] [-H HOURS] [-d DAYSOFWEEK] COMMAND_NAME [ARG_1] ... [ARG_N]\n\
          -> add a new task and print its TASKID\n\
             format & semantics of the \"timing\" fields defined here:\n\
             https://pubs.opengroup.org/onlinepubs/9699919799/utilities/crontab.html\n\
             default value for each field is \"*\"\n\
      or: cassini [OPTIONS] -r TASKID -> remove a task\n\
      or: cassini [OPTIONS] -x TASKID -> get info (time + exit code) on all the past runs of a task\n\
      or: cassini [OPTIONS] -o TASKID -> get the standard output of the last run of a task\n\
      or: cassini [OPTIONS] -e TASKID -> get the standard error\n\
      or: cassini -h -> display this message\n\
\n\
   options:\n\
     -p PIPES_DIR -> look for the pipes in PIPES_DIR (default: /tmp/<USERNAME>/saturnd/pipes)\n\
";

    errno = 0;
    int ret = EXIT_SUCCESS;

    char *minutes_str = "*";
    char *hours_str = "*";
    char *daysofweek_str = "*";
    char *pipes_directory = NULL;

    char *pipe_dir_req = NULL;
    char *pipe_dir_rep = NULL;

    uint16_t operation = CLIENT_REQUEST_LIST_TASKS;
    uint64_t taskid;

    int opt;
    char *strtoull_endp;
    while ((opt = getopt(argc, argv, "hlcqm:H:d:p:r:x:o:e:")) != -1) {
        switch (opt) {
            case 'm':
                minutes_str = optarg;
                break;
            case 'H':
                hours_str = optarg;
                break;
            case 'd':
                daysofweek_str = optarg;
                break;
            case 'p':
                pipes_directory = strdup(optarg);
                if (pipes_directory == NULL) {
                    ret = EXIT_FAILURE;
                    goto error;
                }
                break;
            case 'l':
                operation = CLIENT_REQUEST_LIST_TASKS;
                break;
            case 'c':
                operation = CLIENT_REQUEST_CREATE_TASK;
                break;
            case 'q':
                operation = CLIENT_REQUEST_TERMINATE;
                break;
            case 'r':
                operation = CLIENT_REQUEST_REMOVE_TASK;
                taskid = strtoull(optarg, &strtoull_endp, 10);
                if (strtoull_endp == optarg || strtoull_endp[0] != '\0') {
                    ret = EXIT_FAILURE;
                    goto error;
                }
                break;
            case 'x':
                operation = CLIENT_REQUEST_GET_TIMES_AND_EXITCODES;
                taskid = strtoull(optarg, &strtoull_endp, 10);
                if (strtoull_endp == optarg || strtoull_endp[0] != '\0') {
                    ret = EXIT_FAILURE;
                    goto error;
                }
                break;
            case 'o':
                operation = CLIENT_REQUEST_GET_STDOUT;
                taskid = strtoull(optarg, &strtoull_endp, 10);
                if (strtoull_endp == optarg || strtoull_endp[0] != '\0') {
                    ret = EXIT_FAILURE;
                    goto error;
                }
                break;
            case 'e':
                operation = CLIENT_REQUEST_GET_STDERR;
                taskid = strtoull(optarg, &strtoull_endp, 10);
                if (strtoull_endp == optarg || strtoull_endp[0] != '\0') {
                    ret = EXIT_FAILURE;
                    goto error;
                }
                break;
            case 'h':
                printf("%s", usage_info);
                return 0;
            case '?':
                fprintf(stderr, "%s", usage_info);
                goto error;
        }
    }

    // create paths for reply-pipe and request-pipe
    pipe_dir_req = create_path(pipes_directory, 1);
    pipe_dir_rep = create_path(pipes_directory, 0);

    if ((!pipe_dir_req) || (!pipe_dir_rep)) {
        perror("can't allocate memory!");
        ret = EXIT_FAILURE;
        goto error;
    }

    printf("Req{%s} Rep{%s}\n", pipe_dir_req, pipe_dir_rep);

    // obtain fd for reply-pipe and request-pipe

    /*if ((pipe_req < 0) || (pipe_rep < 0)) {
        perror("open request-pipe failure");
        ret = EXIT_FAILURE;
        goto error;
    }*/

    switch (operation) {
        case CLIENT_REQUEST_CREATE_TASK:
            create_task(pipe_dir_req, pipe_dir_rep, minutes_str, hours_str, daysofweek_str, argc - optind - 1, &argv[optind + 1]);
            break;
        case CLIENT_REQUEST_LIST_TASKS:
            list_task(pipe_dir_req, pipe_dir_rep);
            break;
        case CLIENT_REQUEST_REMOVE_TASK:
            remove_task(pipe_dir_req, pipe_dir_rep, taskid);
            break;
        case CLIENT_REQUEST_GET_STDOUT:
            rq_stdout_stderr(pipe_dir_req, pipe_dir_rep, taskid, operation);
            break;
        case CLIENT_REQUEST_GET_STDERR:
            rq_stdout_stderr(pipe_dir_req, pipe_dir_rep, taskid, operation);
            break;
        case CLIENT_REQUEST_TERMINATE:
            terminate(pipe_dir_req, pipe_dir_rep);
            break;
        case CLIENT_REQUEST_GET_TIMES_AND_EXITCODES:
            times_exitcodes(pipe_dir_req, pipe_dir_rep, taskid);
            break;
    }

error:
    if (errno != 0) {
        perror("main");
    }

    FREE_MEM(pipes_directory);
    FREE_MEM(pipe_dir_req);
    FREE_MEM(pipe_dir_rep);

    return ret;
}
