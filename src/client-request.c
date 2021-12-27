#include "cassini.h"

// function to create paths for request-pipe and reply-pipe
char *create_path(char *pipes_directory, int isRequets) {
    char *pResult = NULL;

    char pPrefix[4096];
    if (!pipes_directory) {
        sprintf(pPrefix, "/tmp/%s/saturnd/pipes", getlogin());
        pipes_directory = pPrefix;
    }

    const char *pDirPostfix = NULL;
    size_t szPipeDir = 0;

    if (isRequets) {
        pDirPostfix = "/saturnd-request-pipe";
        szPipeDir = strlen(pipes_directory) + strlen(pDirPostfix) + 1;
    } else {
        pDirPostfix = "/saturnd-reply-pipe";
        szPipeDir = strlen(pipes_directory) + strlen(pDirPostfix) + 1;
    }

    pResult = malloc(szPipeDir);

    if (pResult) {
        sprintf(pResult, "%s%s", pipes_directory, pDirPostfix);
    }

    return pResult;
}

// function times exitcodes
int times_exitcodes(char* request, char* reply, uint64_t taskid) {
    int retCode = EXIT_SUCCESS;

    // create buffer for request
    void *buf = malloc(2 + sizeof(uint64_t));
    char *bufIter = (char *)buf;
    if (!buf) {
        perror("can't allocate memory");
        retCode = EXIT_FAILURE;
    }

    // write request to buffer
    if (EXIT_SUCCESS == retCode) {
        uint16_t opCode = CLIENT_REQUEST_GET_TIMES_AND_EXITCODES;
        uint64_t tId = taskid;

        opCode = htobe16(opCode);
        tId = htobe64(tId);

        memcpy(bufIter, &opCode, CLIENT_REQUEST_HEADER_SIZE);
        bufIter += CLIENT_REQUEST_HEADER_SIZE;
        memcpy(bufIter, &tId, sizeof(uint64_t));
        bufIter += sizeof(uint64_t);

        // write from buffer to pipe

        int pipe_req = open(request, O_WRONLY);
        ssize_t resRequest = write(pipe_req, buf, 2 + sizeof(uint64_t));
        if (resRequest != 2 + sizeof(uint64_t)) {
            perror("write to pipe failure");
            retCode = EXIT_FAILURE;
        }
        CLOSE_FILE(pipe_req);
    }

    if (EXIT_SUCCESS == retCode) {
        const int lenAnswer = sizeof(uint16_t);
        uint8_t bufReply[lenAnswer];
        int pipe_rep = open(reply, O_RDONLY);
        while (1) {
            ssize_t rezRead = read(pipe_rep, bufReply, lenAnswer);
            if (0 == rezRead)
                continue;                     // no answer, continue to listening
            else if (rezRead == lenAnswer) {  // check if correct response
                uint16_t ResCode = *(uint16_t *)bufReply;

                ResCode = be16toh(ResCode);

                if (ResCode == SERVER_REPLY_OK) {
                    const int lenAnswerNB = sizeof(uint32_t);
                    uint8_t bufReplyNB[lenAnswerNB];
                    while (1) {
                        ssize_t rezReadNB = read(pipe_rep, bufReplyNB, lenAnswerNB);
                        if (0 == rezReadNB)
                            continue;                         // no answer, continue to listening
                        else if (rezReadNB == lenAnswerNB) {  // check if correct response
                            uint32_t nbruns = *(uint32_t *)bufReplyNB;

                            nbruns = be32toh(nbruns);

                            // browse multiple tasks
                            const int lenAnswerOk = sizeof(uint64_t) + sizeof(uint16_t);
                            uint8_t bufReplyOk[lenAnswerOk];
                            for (int i = 0; i < nbruns; ++i) {
                                // retrieve the time spent by task and its id
                                while (1) {
                                    ssize_t rezRead = read(pipe_rep, bufReplyOk, lenAnswerOk);
                                    if (0 == rezRead)
                                        continue;  // no answer, continue to listening
                                    else if (rezRead ==
                                             lenAnswerOk) {  // check if correct response
                                        uint64_t timepass = *(uint64_t *)bufReplyOk;
                                        uint16_t exitcode = *(uint16_t *)(bufReplyOk + 8);

                                        timepass = be64toh(timepass);
                                        exitcode = be16toh(exitcode);

                                        // convert seconds to a date as a string
                                        struct tm *info;
                                        char buffer[80];
                                        info = localtime(&timepass);
                                        strftime(buffer, 80, "%Y-%m-%d %X", info);

                                        printf("%s %hu\n", buffer, exitcode);
                                        break;
                                    } else {  // error
                                        perror("read from pipe-reply failure");
                                        retCode = EXIT_FAILURE;
                                        break;
                                    }
                                }
                            }
                        } else {
                            perror("read from pipe-reply failure");
                            retCode = EXIT_FAILURE;
                            break;
                        }
                        break;
                    }
                } else
                    exit(1);
            } else {
                perror("read from pipe-reply failure");
                retCode = EXIT_FAILURE;
                break;
            }
            break;
        }
        CLOSE_FILE(pipe_rep);
    }
    FREE_MEM(buf);
    return retCode;
}

// function list task
int list_task(char* request, char* reply) {
    int retCode = EXIT_SUCCESS;

    // create buffer for request
    void *buf = malloc(2);
    char *bufIter = (char *)buf;
    if (!buf) {
        perror("can't allocate memory");
        retCode = EXIT_FAILURE;
    }

    // write request to buffer
    if (EXIT_SUCCESS == retCode) {
        uint16_t opCode = CLIENT_REQUEST_LIST_TASKS;

        opCode = htobe16(opCode);

        memcpy(bufIter, &opCode, CLIENT_REQUEST_HEADER_SIZE);
        bufIter += CLIENT_REQUEST_HEADER_SIZE;

        // write from buffer to pipe

        int pipe_req = open(request, O_WRONLY);
        ssize_t resRequest = write(pipe_req, buf, 2);
        if (resRequest != 2) {
            perror("write to pipe failure");
            retCode = EXIT_FAILURE;
        }
        CLOSE_FILE(pipe_req);
    }

    // read request to pipe
    if (EXIT_SUCCESS == retCode) {
        const int lenResponse = sizeof(uint16_t) + sizeof(uint32_t);
        uint8_t bufResponse[lenResponse];
        int pipe_rep = open(reply, O_RDONLY);
        while (1) {
            ssize_t readBuf = read(pipe_rep, bufResponse, lenResponse);
            if (readBuf == 0)
                continue;
            else if (lenResponse == readBuf) {
                uint16_t responseOK = *(uint16_t *)bufResponse;
                uint32_t numberProcess = *(uint32_t *)(bufResponse + 2);

                responseOK = be16toh(responseOK);
                numberProcess = be32toh(numberProcess);

                uint8_t tabTasks[sizeof(uint64_t) + CLIENT_TIMING_SIZE + sizeof(uint32_t)];
                while (numberProcess != 0 != 0) {
                    ssize_t readBufTask = read(pipe_rep, tabTasks, sizeof(uint64_t) + CLIENT_TIMING_SIZE + sizeof(uint32_t));
                    if (readBufTask == 0)
                        continue;
                    else if (sizeof(uint64_t) + CLIENT_TIMING_SIZE + sizeof(uint32_t) == readBufTask) {
                        // retrieve the different tasks
                        for (int m = 0; m < numberProcess; ++m) {
                            uint64_t taskid = *(uint64_t *)(tabTasks);
                            uint64_t timingMin = *(uint64_t *)(tabTasks + 8);
                            uint32_t timingHou = *(uint32_t *)(tabTasks + 8 + 8);
                            uint8_t timingDay = *(uint8_t *)(tabTasks + 8 + 8 + 4);
                            uint32_t cmdArgc = *(uint32_t *)(tabTasks + 8 + 8 + 4 + 1);

                            taskid = be64toh(taskid);
                            timingMin = be64toh(timingMin);
                            timingHou = be32toh(timingHou);
                            cmdArgc = be32toh(cmdArgc);

                            // convert timing to string
                            struct timing t = {.minutes = timingMin,
                                               .hours = timingHou,
                                               .daysofweek = timingDay};
                            char number_str[TIMING_TEXT_MIN_BUFFERSIZE];
                            timing_string_from_timing(number_str, &t);

                            // the number of arguments per task must be at least one
                            if (cmdArgc < 1) {
                                perror("cmdArgc in commandline");
                                retCode = EXIT_FAILURE;
                            }

                            // retrieve all the arguments of the task
                            char *result = malloc(sizeof(char));
                            uint8_t bufsizeword[sizeof(uint32_t)];
                            for (int i = 0; i < cmdArgc; ++i) {
                                ssize_t readsize = read(pipe_rep, bufsizeword, sizeof(uint32_t));
                                if (readsize == -1) {
                                    perror("read from pipe-reply failure");
                                    retCode = EXIT_FAILURE;
                                    break;
                                }

                                // get the size of one of the arguments
                                uint32_t sizeofword = *(uint32_t *)(bufsizeword);

                                sizeofword = be32toh(sizeofword);

                                // Get an argument
                                uint8_t bufword[sizeofword];
                                ssize_t readword = read(pipe_rep, bufword, sizeofword);
                                if (readword == -1) {
                                    perror("read from pipe-reply failure");
                                    retCode = EXIT_FAILURE;
                                    break;
                                }
                                char currArgument[sizeofword + 1];
                                for (int j = 0; j < sizeofword; ++j)
                                    currArgument[j] = (char)(*(uint8_t *)(bufword + j));
                                currArgument[sizeofword] = '\0';

                                // Concatenate arguments
                                size_t const sizeOfAllArg = strlen(result);
                                size_t const sizeOfCurrArg = strlen(currArgument);
                                void *realval =
                                    realloc(result, strlen(result) + strlen(currArgument) + 1);
                                if (!realval) {
                                    perror("realloc failure");
                                    retCode = EXIT_FAILURE;
                                    break;
                                }
                                memcpy(result, result, sizeOfAllArg);
                                if (sizeOfAllArg != 0) {
                                    char *empt = " ";
                                    strcat(result, empt);
                                    memcpy(result + sizeOfAllArg + 1, currArgument,
                                           sizeOfCurrArg + 1);
                                } else
                                    memcpy(result + sizeOfAllArg, currArgument,
                                           sizeOfCurrArg + 1);
                            }

                            // display the task
                            printf("%lu: %s %s\n", taskid, number_str, result);

                            // get the next argument
                            if (m != numberProcess - 1) {
                                readBufTask = read(pipe_rep, tabTasks,
                                                   sizeof(uint64_t) + CLIENT_TIMING_SIZE +
                                                       sizeof(uint32_t));
                                if (readBufTask == -1) {
                                    perror("read from pipe-reply failure");
                                    retCode = EXIT_FAILURE;
                                    break;
                                }
                            } else
                                FREE_MEM(result);
                        }
                        break;
                    } else {
                        perror("read from");
                        retCode = EXIT_FAILURE;
                        break;
                    }
                }
                break;
            } else {
                perror("read from");
                retCode = EXIT_FAILURE;
                break;
            }
        }
        CLOSE_FILE(pipe_rep);
    }
    FREE_MEM(buf);
    return retCode;
}

// function create task
int create_task(char* request, char* reply, char *minutes_str, char *hours_str, char *daysofweek_str, int argc, char *argv[]) {
    int retCode = EXIT_SUCCESS;
    struct timing dest;

    // find the length of request: request's pattern : OPCODE = 'CR' < uint16 >, TIMING<timing>, COMMANDLINE<commandline>
    size_t count = CLIENT_REQUEST_HEADER_SIZE + CLIENT_TIMING_SIZE + sizeof(uint32_t);

    if (argc < 1) {
        perror("no agruments provided");
        return EXIT_FAILURE;
    }
    for (int i = 0; i < argc; i++) {
        count += sizeof(uint32_t) + strlen(argv[i]);
    }

    // create buffer for request
    void *buf = malloc(count);
    char *bufIter = (char *)buf;
    if (!buf) {
        perror("can't allocate memory");
        retCode = EXIT_FAILURE;
    }

    // filing the struct timing
    if (EXIT_SUCCESS == retCode) {
        if (timing_from_strings(&dest, minutes_str, hours_str, daysofweek_str) < 0) {
            perror("timing convertion failure");
            retCode = EXIT_FAILURE;
        }
    }

    // write request to buffer
    if (EXIT_SUCCESS == retCode) {
        uint16_t opCode = CLIENT_REQUEST_CREATE_TASK;
        opCode = htobe16(opCode);
        memcpy(bufIter, &opCode, CLIENT_REQUEST_HEADER_SIZE);
        bufIter += CLIENT_REQUEST_HEADER_SIZE;

        dest.minutes = htobe64(dest.minutes);
        dest.hours = htobe32(dest.hours);
        memcpy(bufIter, &dest.minutes, sizeof(dest.minutes));
        bufIter += sizeof(dest.minutes);
        memcpy(bufIter, &dest.hours, sizeof(dest.hours));
        bufIter += sizeof(dest.hours);
        memcpy(bufIter, &dest.daysofweek, sizeof(dest.daysofweek));
        bufIter += sizeof(dest.daysofweek);

        uint32_t argsC = (uint32_t)argc;
        argsC = htobe32(argsC);        
        memcpy(bufIter, &argsC, sizeof(argsC));
        bufIter += sizeof(argsC);

        for (int i = 0; i < argc; i++) {
            uint32_t len = strlen(argv[i]);
            uint32_t tmp = len;
            tmp = htobe32(len);            

            memcpy(bufIter, &tmp, sizeof(tmp));
            bufIter += sizeof(len);
            memcpy(bufIter, argv[i], len);
            bufIter += len;
        }

        // write from buffer to pipe

        int pipe_req = open(request, O_WRONLY);
        ssize_t resRequest = write(pipe_req, buf, count);
        if (resRequest < (ssize_t)count) {
            perror("write to pipe failure");
            retCode = EXIT_FAILURE;
        }
        CLOSE_FILE(pipe_req);
    }

    // read the answer of saturnd from reply pipe
    // response's pattern : REPTYPE='OK' <uint16>, TASKID <uint64>
    if (EXIT_SUCCESS == retCode) {
        const int lenAnswer = sizeof(uint16_t) + sizeof(uint64_t);
        uint8_t bufReply[lenAnswer];
        int pipe_rep = open(reply, O_RDONLY);
        while (1) {
            ssize_t rezRead = read(pipe_rep, bufReply, lenAnswer);
            if (0 == rezRead)  // no answer, continue to listening
            {
                continue;
            } else if (rezRead == lenAnswer)  // check if correct response
            {
                uint16_t ResCode = *(uint16_t *)bufReply;
                uint64_t uTaskId = *(uint64_t *)(bufReply + 2);
                ResCode = be16toh(ResCode);
                uTaskId = be64toh(uTaskId);                

                // if first 2 bytes = 'OK' it's approved answer
                if (ResCode != SERVER_REPLY_OK) {
                    perror("not approved response");
                    retCode = EXIT_FAILURE;
                    break;
                }
                printf("%lu", uTaskId);
                break;  // correct response
            } else      // error
            {
                perror("read from pipe-reply failure");
                retCode = EXIT_FAILURE;
                break;
            }
        }
        CLOSE_FILE(pipe_rep);
    }
    // free memory
    FREE_MEM(buf);
    return retCode;
}

// function remove task
int remove_task(char* request, char* reply, uint64_t taskid) {
    int retCode = EXIT_SUCCESS;

    // find the length of request: request's pattern : OPCODE='RM' <uint16>, TASKID <uint64>
    size_t count = CLIENT_REQUEST_HEADER_SIZE + sizeof(uint64_t);

    // create buffer for request
    void *buf = malloc(count);
    char *bufIter = (char *)buf;
    if (!buf) {
        perror("can't allocate memory");
        retCode = EXIT_FAILURE;
    }

    // write request to buffer
    if (EXIT_SUCCESS == retCode) {
        uint16_t opCode = CLIENT_REQUEST_REMOVE_TASK;        
        opCode = htobe16(opCode);
        memcpy(bufIter, &opCode, CLIENT_REQUEST_HEADER_SIZE);
        bufIter += CLIENT_REQUEST_HEADER_SIZE;

        uint64_t taskId = (uint64_t)taskid;
        taskId = htobe64(taskId);
        memcpy(bufIter, &taskId, sizeof(taskId));
        bufIter += sizeof(taskId);

        // write from buffer to pipe

        int pipe_req = open(request, O_WRONLY);
        ssize_t resRequest = write(pipe_req, buf, count);
        if (resRequest < (ssize_t)count) {
            perror("write to pipe failure");
            retCode = EXIT_FAILURE;
        }
        CLOSE_FILE(pipe_req);
    }

    // read the answer of saturnd from reply pipe
    // response's pattern : REPTYPE='OK' <uint16>
    //                   or REPTYPE='ER' <uint16>, ERRCODE <uint16> where ERRCODE = 0x4e46 ('NF')
    if (EXIT_SUCCESS == retCode) {
        const int lenAnswer = 2 * sizeof(uint16_t);
        uint32_t bufReply[lenAnswer];
        int pipe_rep = open(reply, O_RDONLY);
        while (1) {
            ssize_t rezRead = read(pipe_rep, bufReply, lenAnswer);
            if (0 == rezRead)  // no answer, continue to listening
            {
                continue;
            } else if (rezRead == lenAnswer)  // check if correct response
            {
                uint16_t ResCode = *(uint16_t *)bufReply;
                uint16_t uErrCode = *(uint16_t *)(bufReply + 2);
                ResCode = be16toh(ResCode);
                uErrCode = be16toh(uErrCode);

                // if first 2 bytes = 'OK' it's approved answer
                if (ResCode != SERVER_REPLY_OK) {
                    // or if first 2 bytes = 'ER'and ERRCODE ='NF' it's approved answer too
                    if (ResCode != SERVER_REPLY_ERROR) {
                        perror("not approved response");
                        retCode = EXIT_FAILURE;
                        break;
                    } else if (uErrCode != SERVER_REPLY_ERROR_NOT_FOUND) {
                        perror("not approved response");
                        retCode = EXIT_FAILURE;
                        break;
                    }
                }
                break;  // correct response
            } else      // error
            {
                perror("read from pipe-reply failure");
                retCode = EXIT_FAILURE;
                break;
            }
        }
        CLOSE_FILE(pipe_rep);
    }
    // free memory
    FREE_MEM(buf);
    return retCode;
}

int rq_stdout_stderr(char* request, char* reply, uint64_t taskid, uint16_t operation) {
    int retCode = EXIT_SUCCESS;
    size_t count = sizeof(uint16_t) + sizeof(uint64_t);
    void *buf = malloc(count);
    char *bufIter = (char *)buf;
    if (!buf) {
        perror("can't allocate memory");
        retCode = EXIT_FAILURE;
    }
    if (EXIT_SUCCESS == retCode) {
        uint16_t opCode = operation;
        uint64_t id = taskid;
        opCode = be16toh(opCode);
        id = be64toh(taskid);

        memcpy(bufIter, &opCode, CLIENT_REQUEST_HEADER_SIZE);
        bufIter += CLIENT_REQUEST_HEADER_SIZE;
        memcpy(bufIter, &id, sizeof(uint64_t));
        bufIter += sizeof(uint64_t);
        int pipe_req = open(request, O_WRONLY);
        ssize_t resRequest = write(pipe_req, buf, count);
        CLOSE_FILE(pipe_req);
        if (resRequest < (ssize_t)count) {
            perror("write to pipe failure");
            retCode = EXIT_FAILURE;
        }
    }
    if (EXIT_SUCCESS == retCode) {
        const int lenAnswer = sizeof(uint16_t);
        uint8_t bufReply[lenAnswer];
        int pipe_rep = open(reply, O_RDONLY);
        while (1) {
            ssize_t resRead = read(pipe_rep, bufReply, lenAnswer);
            if (0 == resRead) {
                continue;
            } else if (resRead == lenAnswer) {
                uint16_t ResCode = *(uint16_t *)bufReply;
                ResCode = be16toh(ResCode);

                if (ResCode == SERVER_REPLY_ERROR) {
                    uint8_t buffReply[sizeof(uint16_t)];
                    ssize_t readErr = read(pipe_rep, buffReply, sizeof(uint16_t));
                    if (readErr == sizeof(uint16_t)) {
                        uint16_t codeErr = *(uint16_t *)buffReply;
                        codeErr = be16toh(codeErr);
                        printf("%d", codeErr);
                        exit(EXIT_FAILURE);
                    } else {
                        perror("read from pipe-reply failure");
                        retCode = EXIT_FAILURE;
                    }

                    break;
                } else {
                    uint8_t buffReply[sizeof(uint32_t)];
                    ssize_t r = read(pipe_rep, buffReply, sizeof(uint32_t));
                    if (r != sizeof(uint32_t))
                        perror("read from pipe-reply failure");
                    else {
                        uint32_t length = *(uint32_t *)buffReply;
                        length = be32toh(length);
                        uint8_t buffer[length + 1];
                        ssize_t readString = read(pipe_rep, buffer, length);
                        buffer[length] = '\0';
                        if (readString != length)
                            perror("read from pipe-reply failure");
                        else
                            printf("%s", buffer);
                    }
                    break;  // correct response
                }
            } else {  // error
                perror("read from pipe-reply failure");
                retCode = EXIT_FAILURE;
                break;
            }
        }
        CLOSE_FILE(pipe_rep);
    }
    FREE_MEM(buf);
    return retCode;
}

// function terminate
int terminate(char* request, char* reply) {

    int retCode = EXIT_SUCCESS;
    size_t count = sizeof(uint16_t);
    void *buf = malloc(count);
    char *bufIter = (char *)buf;
    if (!buf) {
        perror("can't allocate memory");
        retCode = EXIT_FAILURE;
    }
    if (EXIT_SUCCESS == retCode) {
        uint16_t opCode = CLIENT_REQUEST_TERMINATE;
        opCode = be16toh(opCode);

        memcpy(bufIter, &opCode, CLIENT_REQUEST_HEADER_SIZE);
        bufIter += CLIENT_REQUEST_HEADER_SIZE;
        int pipe_req = open(request, O_WRONLY);
        ssize_t resRequest = write(pipe_req, buf, count);
        if (resRequest < (ssize_t)count) {
            perror("write to pipe failure");
            retCode = EXIT_FAILURE;
        }
        CLOSE_FILE(pipe_req);
    }
    if (EXIT_SUCCESS == retCode) {
        const int lenAnswer = sizeof(uint16_t);
        uint8_t bufReply[lenAnswer];
        int pipe_rep = open(reply, O_RDONLY);
        while (1) {
            ssize_t rezRead = read(pipe_rep, bufReply, lenAnswer);
            if (0 == rezRead)  // no answer, continue to listening
            {
                continue;
            } else if (rezRead == lenAnswer)  // check if correct response
            {
                uint16_t ResCode = *(uint16_t *)bufReply;
                ResCode = be16toh(ResCode);

                // if first 2 bytes = 'OK' it's approved answer
                if (ResCode != SERVER_REPLY_OK) {
                    perror("not approved response");
                    retCode = EXIT_FAILURE;
                    break;
                }
                break;
                break;  // correct response
            } else      // error
            {
                perror("read from pipe-reply failure");
                retCode = EXIT_FAILURE;
                break;
            }
        }
        CLOSE_FILE(pipe_rep);
    }
    FREE_MEM(buf);
    return retCode;
}