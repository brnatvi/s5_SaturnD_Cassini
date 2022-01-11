#ifndef PIPES_PATH_H
#define PIPES_PATH_H
                     
#define GET_DEFAULT_PATH(buff, postfix) sprintf(buff, "/tmp/%s/saturnd/%s", getlogin(), postfix);

#define PIPE_REQUEST_NAME "/saturnd-request-pipe" 
#define PIPE_REPLY_NAME   "/saturnd-reply-pipe"
                     
#endif //PIPES_PATH_H
