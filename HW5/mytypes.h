#ifndef MYTYPES_H
#define MYTYPES_H

typedef struct Directories
{
    char source[256];
    char destination[256];
    int bufferSize;
    int dirCount;
} Directories;

typedef struct FileInfoBuffer
{
    char fileName[256];
    int sourceFd;
    int destFd;

} FileInfoBuffer;

#endif