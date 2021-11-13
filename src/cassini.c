#include "cassini.h"
#include "timing-text-io.h"
#include "timing.h"

const char usage_info[] = "\
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

// function to check if big endian on
int isBigEndian()
{
    uint16_t testVal = 1;
    uint8_t *pTestVal = (uint8_t *)&testVal;
    return (pTestVal[0] != 1) ? 1 : 0;
}

// function to create paths for request-pipe and reply-pipe
char *create_path(char *pipes_directory, int isRequets)
{
    char *pResult = NULL;

    char pPrefix[4096];
    if (!pipes_directory)
    {
        sprintf(pPrefix, "/tmp/%s/saturnd/pipes", getlogin());
        pipes_directory = pPrefix;
    }

    const char *pDirPostfix = NULL;
    size_t szPipeDir = 0;

    if (isRequets)
    {
        pDirPostfix = "/saturnd-request-pipe";
        szPipeDir = strlen(pipes_directory) + strlen(pDirPostfix) + 1;
    }
    else
    {
        pDirPostfix = "/saturnd-reply-pipe";
        szPipeDir = strlen(pipes_directory) + strlen(pDirPostfix) + 1;
    }

    pResult = malloc(szPipeDir);

    if (pResult)
    {
        sprintf(pResult, "%s%s", pipes_directory, pDirPostfix);
    }

    return pResult;
}

// function create task
int create_task(int request, int reply, char *minutes_str, char *hours_str, char *daysofweek_str, int argc, char *argv[])
{
    int retCode = EXIT_SUCCESS;
    struct timing dest;

    int isBigE = isBigEndian(); // check if need to convert to big endian

    // find the length of request: request's pattern : OPCODE = 'CR' < uint16 >, TIMING<timing>, COMMANDLINE<commandline>
    size_t count = 2 * sizeof(char) + 13 //sizeof(dest) - we can't use sizeof because of different alignment depending on platform and compiler options
                   + sizeof(uint32_t);

    if (argc < 1)
    {
        perror("no agruments provided");
        return EXIT_FAILURE;
    }
    for (int i = 0; i < argc; i++)
    {
        count += sizeof(uint32_t) + strlen(argv[i]);
    }

    // create buffer for request
    void *buf = malloc(count);
    char *bufIter = (char *)buf;
    if (!buf)
    {
        perror("can't allocate memory");
        retCode = EXIT_FAILURE;
    }

    // filing the struct timing
    if (EXIT_SUCCESS == retCode)
    {
        if (timing_from_strings(&dest, minutes_str, hours_str, daysofweek_str) < 0)
        {
            perror("timing convertion failure");
            retCode = EXIT_FAILURE;
        }
    }

    // write request to buffer
    if (EXIT_SUCCESS == retCode)
    {
        uint16_t opCode = CLIENT_REQUEST_CREATE_TASK;
        if (!isBigE)
        {
            opCode = htobe16(opCode);
        }

        memcpy(bufIter, &opCode, CLIENT_REQUEST_HEADER_SIZE);
        bufIter += CLIENT_REQUEST_HEADER_SIZE;

        if (!isBigE)
        {
            dest.minutes = htobe64(dest.minutes);
            dest.hours = htobe32(dest.hours);
        }

        memcpy(bufIter, &dest.minutes, sizeof(dest.minutes));
        bufIter += sizeof(dest.minutes);
        memcpy(bufIter, &dest.hours, sizeof(dest.hours));
        bufIter += sizeof(dest.hours);
        memcpy(bufIter, &dest.daysofweek, sizeof(dest.daysofweek));
        bufIter += sizeof(dest.daysofweek);

        uint32_t argsC = (uint32_t)argc;
        if (!isBigE)
        {
            argsC = htobe32(argsC);
        }
        memcpy(bufIter, &argsC, sizeof(argsC));
        bufIter += sizeof(argsC);

        for (int i = 0; i < argc; i++)
        {
            uint32_t len = strlen(argv[i]);
            uint32_t tmp = len;

            if (!isBigE)
            {
                tmp = htobe32(len);
            }

            memcpy(bufIter, &tmp, sizeof(tmp));
            bufIter += sizeof(len);
            memcpy(bufIter, argv[i], len);
            bufIter += len;
        }

        // write from buffer to pipe
        ssize_t resRequest = write(request, buf, count);
        if (resRequest < (ssize_t)count)
        {
            perror("write to pipe failure");
            retCode = EXIT_FAILURE;
        }
    }

    // read the answer of saturnd from reply pipe
    // response's pattern : REPTYPE='OK' <uint16>, TASKID <uint64>
    if (EXIT_SUCCESS == retCode)
    {
        const int lenAnswer = sizeof(uint16_t) + sizeof(uint64_t);
        uint8_t bufReply[lenAnswer];
        while (1)
        {
            ssize_t rezRead = read(reply, bufReply, lenAnswer);
            if (0 == rezRead) // no answer, continue to listening
            {
                continue;
            }
            else if (rezRead == lenAnswer) // check if correct response
            {
                uint16_t ResCode = *(uint16_t *)bufReply;
                uint64_t uTaskId = *(uint64_t *)(bufReply + 2);

                if (!isBigE)
                {
                    ResCode = htobe16(ResCode);
                    uTaskId = htobe64(uTaskId);
                }

                // if first 2 bytes = 'OK' it's approved answer
                if (ResCode != SERVER_REPLY_OK)
                {
                    perror("not approved response");
                    retCode = EXIT_FAILURE;
                    break;
                }

                printf("%lu", uTaskId);

                break; // correct response
            }
            else // error
            {
                perror("read from pipe-reply failure");
                retCode = EXIT_FAILURE;
                break;
            }
        }
    }

    // free memory
    FREE_MEM(buf);

    return retCode;
}

int main(int argc, char *argv[])
{
    errno = 0;
    int ret = EXIT_SUCCESS;

    char *minutes_str = "*";
    char *hours_str = "*";
    char *daysofweek_str = "*";
    char *pipes_directory = NULL;

    char *pipe_dir_req = NULL;
    char *pipe_dir_rep = NULL;

    int pipe_req = -1;
    int pipe_rep = -1;

    uint16_t operation = CLIENT_REQUEST_LIST_TASKS;
    uint64_t taskid;

    int opt;
    char *strtoull_endp;
    while ((opt = getopt(argc, argv, "hlcqm:H:d:p:r:x:o:e:")) != -1)
    {
        switch (opt)
        {
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
            if (pipes_directory == NULL)
            {
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
            if (strtoull_endp == optarg || strtoull_endp[0] != '\0')
            {
                ret = EXIT_FAILURE;
                goto error;
            }
            break;
        case 'x':
            operation = CLIENT_REQUEST_GET_TIMES_AND_EXITCODES;
            taskid = strtoull(optarg, &strtoull_endp, 10);
            if (strtoull_endp == optarg || strtoull_endp[0] != '\0')
            {
                ret = EXIT_FAILURE;
                goto error;
            }
            break;
        case 'o':
            operation = CLIENT_REQUEST_GET_STDOUT;
            taskid = strtoull(optarg, &strtoull_endp, 10);
            if (strtoull_endp == optarg || strtoull_endp[0] != '\0')
            {
                ret = EXIT_FAILURE;
                goto error;
            }
            break;
        case 'e':
            operation = CLIENT_REQUEST_GET_STDERR;
            taskid = strtoull(optarg, &strtoull_endp, 10);
            if (strtoull_endp == optarg || strtoull_endp[0] != '\0')
            {
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
    if ((!pipe_dir_req) || (!pipe_dir_rep))
    {
        perror("can't allocate memory!");
        ret = EXIT_FAILURE;
        goto error;
    }

    // obtain fd for reply-pipe and request-pipe
    pipe_req = open(pipe_dir_req, O_WRONLY);
    pipe_rep = open(pipe_dir_rep, O_RDONLY);
    if ((pipe_req < 0) || (pipe_rep < 0))
    {
        perror("open request-pipe failure");
        ret = EXIT_FAILURE;
        goto error;
    }

    switch (operation)
    {
    case CLIENT_REQUEST_CREATE_TASK:
        create_task(pipe_req, pipe_rep, minutes_str, hours_str, daysofweek_str, argc - optind, &argv[optind]);
        break;
    }

error:
    if (errno != 0)
    {
        perror("main");
    }

    FREE_MEM(pipes_directory);
    FREE_MEM(pipe_dir_req);
    FREE_MEM(pipe_dir_rep);
    CLOSE_FILE(pipe_req);
    CLOSE_FILE(pipe_rep);

    return ret;
}
