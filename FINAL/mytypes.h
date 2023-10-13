#ifndef MYTYPES_H
#define MYTYPES_H
#include <sys/socket.h>

#define MAX_FILE 50


typedef struct ClientNode
{
    int sockFd;
    char addr[25];

    struct ClientNode *next;

} ClientNode;

typedef struct File
{
    char name[256];
    char path[256];
    int ownerId;
    long int mTime; // modification time
    bool deleted;
    int type;
    bool marked;
    struct File *(files[MAX_FILE]);
} File;

#endif