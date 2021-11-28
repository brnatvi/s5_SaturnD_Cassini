#include "cassini.h"

// function to check if big endian on
int isBigEndian() {
    uint16_t testVal = 1;
    uint8_t *pTestVal = (uint8_t *)&testVal;
    return (pTestVal[0] != 1) ? 1 : 0;
}

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
int times_exitcodes(int request, int reply, uint64_t taskid, int isBigE) {
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
        if (!isBigE) {
            opCode = htobe16(opCode);
            tId = htobe64(tId);
        }

        memcpy(bufIter, &opCode, CLIENT_REQUEST_HEADER_SIZE);
        bufIter += CLIENT_REQUEST_HEADER_SIZE;
        memcpy(bufIter, &tId, sizeof(uint64_t));
        bufIter += sizeof(uint64_t);

        // write from buffer to pipe
        ssize_t resRequest = write(request, buf, 2 + sizeof(uint64_t));
        if (resRequest != 2 + sizeof(uint64_t)) {
            perror("write to pipe failure");
            retCode = EXIT_FAILURE;
        }
    }

    if (EXIT_SUCCESS == retCode) {
        const int lenAnswer = sizeof(uint16_t);
        uint8_t bufReply[lenAnswer];
        while (1) {
            ssize_t rezRead = read(reply, bufReply, lenAnswer);
            if (0 == rezRead)
                continue;                     // no answer, continue to listening
            else if (rezRead == lenAnswer) {  // check if correct response
                uint16_t ResCode = *(uint16_t *)bufReply;
                if (!isBigE) {
                    ResCode = htobe16(ResCode);
                }

                if (ResCode == SERVER_REPLY_OK) {
                    const int lenAnswerNB = sizeof(uint32_t);
                    uint8_t bufReplyNB[lenAnswerNB];
                    while (1) {
                        ssize_t rezReadNB = read(reply, bufReplyNB, lenAnswerNB);
                        if (0 == rezReadNB)
                            continue;                         // no answer, continue to listening
                        else if (rezReadNB == lenAnswerNB) {  // check if correct response
                            uint32_t nbruns = *(uint32_t *)bufReplyNB;
                            if (!isBigE) {
                                nbruns = htobe32(nbruns);
                            }

                            // browse multiple tasks
                            const int lenAnswerOk = sizeof(uint64_t) + sizeof(uint16_t);
                            uint8_t bufReplyOk[lenAnswerOk];
                            for (int i = 0; i < nbruns; ++i) {
                                // retrieve the time spent by task and its id
                                while (1) {
                                    ssize_t rezRead = read(reply, bufReplyOk, lenAnswerOk);
                                    if (0 == rezRead)
                                        continue;  // no answer, continue to listening
                                    else if (rezRead ==
                                             lenAnswerOk) {  // check if correct response
                                        uint64_t timepass = *(uint64_t *)bufReplyOk;
                                        uint16_t exitcode = *(uint16_t *)(bufReplyOk + 8);

                                        if (!isBigE) {
                                            timepass = htobe64(timepass);
                                            exitcode = htobe16(exitcode);
                                        }

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
    }
    FREE_MEM(buf);
    return retCode;
}

// function list task
int list_task(int request, int reply, int isBigE) {
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
        if (!isBigE) {
            opCode = htobe16(opCode);
        }

        memcpy(bufIter, &opCode, CLIENT_REQUEST_HEADER_SIZE);
        bufIter += CLIENT_REQUEST_HEADER_SIZE;

        // write from buffer to pipe
        ssize_t resRequest = write(request, buf, 2);
        if (resRequest != 2) {
            perror("write to pipe failure");
            retCode = EXIT_FAILURE;
        }
    }

    // read request to pipe
    if (EXIT_SUCCESS == retCode) {
        const int lenResponse = sizeof(uint16_t) + sizeof(uint32_t);
        uint8_t bufResponse[lenResponse];
        while (1) {
            ssize_t readBuf = read(reply, bufResponse, lenResponse);
            if (readBuf == 0)
                continue;
            else if (lenResponse == readBuf) {
                uint16_t responseOK = *(uint16_t *)bufResponse;
                uint32_t numberProcess = *(uint32_t *)(bufResponse + 2);
                if (!isBigE) {
                    responseOK = htobe16(responseOK);
                    numberProcess = htobe32(numberProcess);
                }

                uint8_t tabTasks[sizeof(uint64_t) + CLIENT_TIMING_SIZE + sizeof(uint32_t)];
                while (numberProcess != 0 != 0) {
                    ssize_t readBufTask = read(reply, tabTasks, sizeof(uint64_t) + CLIENT_TIMING_SIZE + sizeof(uint32_t));
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

                            if (!isBigE) {
                                taskid = htobe64(taskid);
                                timingMin = htobe64(timingMin);
                                timingHou = htobe32(timingHou);
                                cmdArgc = htobe32(cmdArgc);
                            }

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
                                ssize_t readsize = read(reply, bufsizeword, sizeof(uint32_t));
                                if (readsize == -1) {
                                    perror("read from pipe-reply failure");
                                    retCode = EXIT_FAILURE;
                                    break;
                                }

                                // get the size of one of the arguments
                                uint32_t sizeofword = *(uint32_t *)(bufsizeword);
                                if (!isBigE) {
                                    sizeofword = htobe32(sizeofword);
                                }

                                // Get an argument
                                uint8_t bufword[sizeofword];
                                ssize_t readword = read(reply, bufword, sizeofword);
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
                                readBufTask = read(reply, tabTasks,
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
    }
    FREE_MEM(buf);
    return retCode;
}

// function create task
int create_task(int request, int reply, char *minutes_str, char *hours_str, char *daysofweek_str, int argc, char *argv[], int isBigE) {
    int retCode = EXIT_SUCCESS;
    struct timing dest;

    // find the length of request: request's pattern : OPCODE = 'CR' < uint16 >, TIMING<timing>, COMMANDLINE<commandline>
    size_t count = 2 * sizeof(char) +
                   CLIENT_TIMING_SIZE  //sizeof(dest) - we can't use sizeof because of different alignment depending on platform and compiler
                   + sizeof(uint32_t);

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
        if (!isBigE) {
            opCode = htobe16(opCode);
        }

        memcpy(bufIter, &opCode, CLIENT_REQUEST_HEADER_SIZE);
        bufIter += CLIENT_REQUEST_HEADER_SIZE;

        if (!isBigE) {
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
        if (!isBigE) {
            argsC = htobe32(argsC);
        }
        memcpy(bufIter, &argsC, sizeof(argsC));
        bufIter += sizeof(argsC);

        for (int i = 0; i < argc; i++) {
            uint32_t len = strlen(argv[i]);
            uint32_t tmp = len;

            if (!isBigE) {
                tmp = htobe32(len);
            }

            memcpy(bufIter, &tmp, sizeof(tmp));
            bufIter += sizeof(len);
            memcpy(bufIter, argv[i], len);
            bufIter += len;
        }

        // write from buffer to pipe
        ssize_t resRequest = write(request, buf, count);
        if (resRequest < (ssize_t)count) {
            perror("write to pipe failure");
            retCode = EXIT_FAILURE;
        }
    }

    // read the answer of saturnd from reply pipe
    // response's pattern : REPTYPE='OK' <uint16>, TASKID <uint64>
    if (EXIT_SUCCESS == retCode) {
        const int lenAnswer = sizeof(uint16_t) + sizeof(uint64_t);
        uint8_t bufReply[lenAnswer];
        while (1) {
            ssize_t rezRead = read(reply, bufReply, lenAnswer);
            if (0 == rezRead)  // no answer, continue to listening
            {
                continue;
            } else if (rezRead == lenAnswer)  // check if correct response
            {
                uint16_t ResCode = *(uint16_t *)bufReply;
                uint64_t uTaskId = *(uint64_t *)(bufReply + 2);

                if (!isBigE) {
                    ResCode = htobe16(ResCode);
                    uTaskId = htobe64(uTaskId);
                }

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
    }
    // free memory
    FREE_MEM(buf);

    return retCode;
}

// function remove task
int remove_task(int request, int reply, uint64_t taskid, int isBigE) {
    int retCode = EXIT_SUCCESS;

    // find the length of request: request's pattern : OPCODE='RM' <uint16>, TASKID <uint64>
    size_t count = 2 * sizeof(char) + sizeof(uint64_t);

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
        if (!isBigE) {
            opCode = htobe16(opCode);
        }

        memcpy(bufIter, &opCode, CLIENT_REQUEST_HEADER_SIZE);
        bufIter += CLIENT_REQUEST_HEADER_SIZE;

        uint64_t taskId = (uint64_t)taskid;
        if (!isBigE) {
            taskId = htobe64(taskId);
        }
        memcpy(bufIter, &taskId, sizeof(taskId));
        bufIter += sizeof(taskId);

        // write from buffer to pipe
        ssize_t resRequest = write(request, buf, count);
        if (resRequest < (ssize_t)count) {
            perror("write to pipe failure");
            retCode = EXIT_FAILURE;
        }
    }

    // read the answer of saturnd from reply pipe
    // response's pattern : REPTYPE='OK' <uint16>
    //                   or REPTYPE='ER' <uint16>, ERRCODE <uint16>
    //                                             ERRCODE = 0x4e46 ('NF')
    if (EXIT_SUCCESS == retCode) {
        const int lenAnswer = 2 * sizeof(uint16_t);
        uint32_t bufReply[lenAnswer];
        while (1) {
            ssize_t rezRead = read(reply, bufReply, lenAnswer);
            if (0 == rezRead)  // no answer, continue to listening
            {
                continue;
            } else if (rezRead == lenAnswer)  // check if correct response
            {
                uint16_t ResCode = *(uint16_t *)bufReply;
                uint16_t uErrCode = *(uint16_t *)(bufReply + 2);

                if (!isBigE) {
                    ResCode = htobe16(ResCode);
                    uErrCode = htobe16(uErrCode);
                }

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

                // printf("%u", ResCode);

                break;  // correct response
            } else      // error
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

int rq_stdout_stderr(int request, int reply, uint64_t taskid, uint16_t operation, int isBigE) {
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
        if (!isBigE) {
            opCode = htobe16(opCode);
            id = htobe64(taskid);
        }

        memcpy(bufIter, &opCode, CLIENT_REQUEST_HEADER_SIZE);
        bufIter += CLIENT_REQUEST_HEADER_SIZE;
        memcpy(bufIter, &id, sizeof(uint64_t));
        bufIter += sizeof(uint64_t);
        ssize_t resRequest = write(request, buf, count);
        if (resRequest < (ssize_t)count) {
            perror("write to pipe failure");
            retCode = EXIT_FAILURE;
        }
    }
    if (EXIT_SUCCESS == retCode) {
        const int lenAnswer = sizeof(uint16_t);
        uint8_t bufReply[lenAnswer];
        while (1) {
            ssize_t resRead = read(reply, bufReply, lenAnswer);
            if (0 == resRead) {
                continue;
            } else if (resRead == lenAnswer) {
                uint16_t ResCode = *(uint16_t *)bufReply;
                if (!isBigE)
                    ResCode = htobe16(ResCode);

                if (ResCode == SERVER_REPLY_ERROR) {
                    uint8_t buffReply[sizeof(uint16_t)];
                    ssize_t readErr = read(reply, buffReply, sizeof(uint16_t));
                    if (readErr == sizeof(uint16_t)) {
                        uint16_t codeErr = *(uint16_t *)buffReply;
                        if (!isBigE)
                            codeErr = htobe16(codeErr);
                        printf("%d", codeErr);
                        exit(EXIT_FAILURE);
                    } else {
                        perror("read from pipe-reply failure");
                        retCode = EXIT_FAILURE;
                    }

                    break;
                } else {
                    uint8_t buffReply[sizeof(uint32_t)];
                    ssize_t r = read(reply, buffReply, sizeof(uint32_t));
                    if (r != sizeof(uint32_t))
                        perror("read from pipe-reply failure");
                    else {
                        uint32_t length = *(uint32_t *)buffReply;
                        if (!isBigE)
                            length = htobe32(length);
                        uint8_t buffer[length + 1];
                        ssize_t readString = read(reply, buffer, length);
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
    }
    FREE_MEM(buf);
    return retCode;
}

// function terminate
int terminate(int request, int reply, int isBigE) {
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
        if (!isBigE) {
            opCode = htobe16(opCode);
        }

        memcpy(bufIter, &opCode, CLIENT_REQUEST_HEADER_SIZE);
        bufIter += CLIENT_REQUEST_HEADER_SIZE;
        ssize_t resRequest = write(request, buf, count);
        if (resRequest < (ssize_t)count) {
            perror("write to pipe failure");
            retCode = EXIT_FAILURE;
        }
    }
    if (EXIT_SUCCESS == retCode) {
        const int lenAnswer = sizeof(uint16_t);
        uint8_t bufReply[lenAnswer];
        while (1) {
            ssize_t rezRead = read(reply, bufReply, lenAnswer);
            if (0 == rezRead)  // no answer, continue to listening
            {
                continue;
            } else if (rezRead == lenAnswer)  // check if correct response
            {
                uint16_t ResCode = *(uint16_t *)bufReply;

                if (!isBigE) {
                    ResCode = htobe16(ResCode);
                }

                // if first 2 bytes = 'OK' it's approved answer
                if (ResCode != SERVER_REPLY_OK) {
                    perror("not approved response");
                    retCode = EXIT_FAILURE;
                    break;
                }
                break;  // correct response
            } else      // error
            {
                perror("read from pipe-reply failure");
                retCode = EXIT_FAILURE;
                break;
            }
        }
    }
    FREE_MEM(buf);
    return retCode;
}