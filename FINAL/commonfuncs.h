#ifndef COMMONFUNCS_H
#define COMMONFUNCS_H
#ifndef UTIME_NOW
#define UTIME_NOW ((1l << 30) - 1l)
#endif
#ifndef AT_FDCWD
#define AT_FDCWD -100
#endif
#define MAX 1024
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "filefuncs.h"
#include <errno.h>
// append the name of the files in given directory to the given list
void ListFiles(char dirPath[], char filePath[], File **head)
{
    DIR *dir;
    struct dirent *entry;
    struct stat finfo;
    char tempPath[MAX];
    if (!(dir = opendir(dirPath)))
        return;

    while ((entry = readdir(dir)) != NULL)
    {
        // exclude current and parent directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        else if (entry->d_type == 4)
        {
            char subDir[MAX];
            snprintf(subDir, sizeof(subDir), "%s/%s", dirPath, entry->d_name);
            if (strcmp(filePath, "/") == 0)
                snprintf(tempPath, sizeof(tempPath), "%s", entry->d_name);
            else
                snprintf(tempPath, sizeof(tempPath), "%s/%s", filePath, entry->d_name);

            AppendFile(head, tempPath, 0, 0, 4);
            ListFiles(subDir, tempPath, head);
            continue;
        }
        else if (entry->d_type == 8)
        {
            if (strcmp(filePath, "/") == 0)
            {
                snprintf(tempPath, sizeof(tempPath), "%s", entry->d_name);
            }
            else
            {
                snprintf(tempPath, sizeof(tempPath), "%s/%s", filePath, entry->d_name);
            }
            AppendFile(head, tempPath, 0, 0, 8);
        }
    }
    closedir(dir);
}

// Changes given files modification time, when file transferred its modification time should not change
void ChangeFileMTime(char filePath[], long int seconds)
{
    struct timespec times[2];
    // Set the access time to the current time it is not important, server checks the modification time
    times[0].tv_sec = 0;
    times[0].tv_nsec = UTIME_NOW;
    // Set the modification time to the same as client because file is not modified actually
    times[1]
        .tv_sec = seconds;
    times[1].tv_nsec = 0;
    utimensat(AT_FDCWD, filePath, times, 0);
}

void sendLong(int sock, long val)
{
    uint32_t networkVal = htonl(val);
    write(sock, &networkVal, sizeof(networkVal));
}
long rcvLong(int sock)
{
    uint32_t tmp, n;
    read(sock, &tmp, sizeof(tmp));
    n = ntohl(tmp);
    return n;
}
void sendMsg(int sock, char msg[])
{
    uint16_t len = strlen(msg);
    uint16_t networkLen = htons(len); // convert to network byte order

    write(sock, &networkLen, sizeof(networkLen));
    write(sock, msg, len);
}
void sendSizedMsg(int sock, char msg[], short len)
{
    uint16_t networkLen = htons(len); // convert to network byte order

    write(sock, &networkLen, sizeof(networkLen));
    write(sock, msg, len);
}
char *receiveMsg(int sock)
{

    uint16_t networkLen;
    read(sock, &networkLen, sizeof(networkLen));

    uint16_t len = ntohs(networkLen); // convert back to host byte order

    char *msgBuffer = malloc((sizeof(char) * len) + 1);
    read(sock, msgBuffer, len);

    msgBuffer[len] = '\0';

    return msgBuffer;
}
char *receiveSizedMsg(int sock, short *size)
{

    uint16_t networkLen;
    read(sock, &networkLen, sizeof(networkLen));

    uint16_t len = ntohs(networkLen); // convert back to host byte order

    char *msgBuffer = malloc(sizeof(char) * len);
    *size = read(sock, msgBuffer, len);

    // msgBuffer[len] = '\0';

    return msgBuffer;
}

// create the given dir recursively
void CreateDirectory(char rootdir[], char dirPath[])
{
    char *currentIndex, *savePtr;
    struct stat finfo;
    char absDirPath[MAX], relDirPath[MAX];

    currentIndex = strtok_r(dirPath, "/", &savePtr);
    sprintf(absDirPath, "%s/%s", rootdir, currentIndex);
    strcpy(relDirPath, currentIndex);

    // if directory does not exist, create it
    if (stat(absDirPath, &finfo) != 0)
    {
        int res = mkdir(absDirPath, S_IRWXU | S_IRWXG | S_IRWXO);
        if (res == -1)
        {
            if (errno != 17)
            {

                printf("Error1:%s (%s)\n", strerror(errno), absDirPath);
                exit(-1);
            }
            else
            {
                errno = 0;
            }
        }
    }
    while (currentIndex != NULL)
    {
        currentIndex = strtok_r(NULL, "/", &savePtr);
        if (currentIndex == NULL)
            break;

        strcat(absDirPath, "/");
        strcat(absDirPath, currentIndex);

        strcat(relDirPath, "/");
        strcat(relDirPath, currentIndex);

        // if directory does not exist, create it
        if (stat(absDirPath, &finfo) != 0)
        {
            int res = mkdir(absDirPath, S_IRWXU | S_IRWXG | S_IRWXO);
            if (res == -1)
            {
                if (errno != 17)
                {

                    printf("Error2:%s (%s)\n", strerror(errno), absDirPath);
                    exit(-1);
                }
                else
                {
                    errno = 0;
                }
            }
        }
    }
}
// recursively remove the directory whose path is given
void *RemoveDirectory(char dirPath[])
{

    DIR *dir;
    struct dirent *entry;
    char tempPath[MAX];
    if (!(dir = opendir(dirPath)))
        return NULL;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char subDirPath[1024];

        snprintf(subDirPath, sizeof(subDirPath), "%s/%s", dirPath, entry->d_name);

        if (entry->d_type == 4)
            RemoveDirectory(subDirPath);

        else if (entry->d_type == 8)
            remove(subDirPath);
        
    }
    closedir(dir);
    rmdir(dirPath);
}


#endif
