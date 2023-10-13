#ifndef MYTYPES_H
#define MYTYPES_H
#include "myconst.h"

typedef struct Task
{
    int pid;
    char request[MESSAGE_LEN];
} Task;

struct taskNode
{
    Task task;
    struct taskNode *next;
};
struct pidNode
{
    int pid;
    struct pidNode *next;
};

struct request {
    pid_t pid;
    char message[MESSAGE_LEN];
};

struct response{
    char message[MESSAGE_LEN];
    int len;
};

typedef struct ClientInfo{
    int pid;
    int num;
} ClientInfo;

#endif