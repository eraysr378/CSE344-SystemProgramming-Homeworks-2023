#define _POSIX_C_SOURCE 200112L
#ifndef RECURSIVE_H
#define RECURSIVE_H

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include "globals.h"
#include "mytypes.h"
#include "clientqueue.h"
#include <string.h>
#include <dirent.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "filefuncs.h"
#include "commonfuncs.h"

// transfer up to date  files from server to client at connection
void *TransferFilesAtConnection(char dirPath[], char pathToSend[], int clfd, int clientId)
{
    FILE *fp;
    short readBytes;
    char absFilePath[1024], filePathToSend[1024], msg[1024], *msgPtr;
    struct stat finfo;
    long file_mTime;
    DIR *dir;
    struct dirent *entry;
    if (!(dir = opendir(dirPath)))
        return NULL;

    while ((entry = readdir(dir)) != NULL)
    { // exclude current and parent directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        // Another directory (subdirectory) found in the directory
        if (entry->d_type == 4)
        {
            char subDirPath[1024];
            // store the path to traverse subdirectory
            snprintf(subDirPath, sizeof(subDirPath), "%s/%s", dirPath, entry->d_name);
            if (strcmp(pathToSend, "/") == 0)
                snprintf(filePathToSend, sizeof(filePathToSend), "%s", entry->d_name);
            else
                snprintf(filePathToSend, sizeof(filePathToSend), "%s/%s", pathToSend, entry->d_name);

            // send file name/path
            sendMsg(clfd, filePathToSend);
            // send file type
            sendLong(clfd, (long)4);

            AppendFile(&listUpdated, filePathToSend, -1, 0, entry->d_type);
            // recursively traverse the subdirectory
            TransferFilesAtConnection(subDirPath, filePathToSend, clfd, clientId);

            continue;
        }
        else if (entry->d_type == 8)
        {
            short readBytes;
            char readBuff[1024];
            char buff[1024];

            if (strcmp(pathToSend, "/") == 0)
                snprintf(filePathToSend, sizeof(filePathToSend), "%s", entry->d_name);
            else
                snprintf(filePathToSend, sizeof(filePathToSend), "%s/%s", pathToSend, entry->d_name);

            snprintf(absFilePath, sizeof(absFilePath), "%s/%s", dirPath, entry->d_name);

            // send file name
            sendMsg(clfd, filePathToSend);
            // send file type
            sendLong(clfd, (long)8);

            // wait for response to figure if file exists in client as well
            msgPtr = receiveMsg(clfd);

            if (strcmp(msgPtr, "file exists") == 0)
            {

                free(msgPtr);
                stat(absFilePath, &finfo); // finfo.st_mtime

                sendLong(clfd, finfo.st_mtime);
                msgPtr = receiveMsg(clfd);
                // wait for client response, who receive who send?
                if (strcmp(msgPtr, "client sending") == 0)
                {
                    free(msgPtr);
                    // receive modification time
                    long cl_mTime = rcvLong(clfd);

                    fp = fopen(absFilePath, "wb");
                    while (1)
                    {
                        // store as byte and string
                        msgPtr = receiveSizedMsg(clfd, &readBytes);
                        strcpy(buff, msgPtr);
                        memcpy(readBuff, msgPtr, readBytes);
                        free(msgPtr);
                        if (strcmp(buff, "FILE SENT1") == 0)
                        {
                            printf("FILE RECEIVED1\n");
                            break;
                        }
                        fwrite(readBuff, 1, readBytes, fp);
                    }
                    msgPtr = receiveMsg(clfd);
                    fclose(fp);
                    if (strcmp(msgPtr, "DELETE") == 0)
                    {
                        remove(absFilePath);
                    }
                    else
                    {
                        ChangeFileMTime(absFilePath, cl_mTime);
                        AppendFile(&listUpdated, filePathToSend, clientId, cl_mTime, entry->d_type);
                    }
                    free(msgPtr);
                }
                else
                {
                    free(msgPtr);
                    fp = fopen(absFilePath, "rb");
                    while (fp != NULL && (readBytes = fread(buff, 1, sizeof(buff), fp)) > 0)
                    {
                        sendSizedMsg(clfd, buff, readBytes);
                    }
                    sendMsg(clfd, "FILE SENT2");
                    if (fp == NULL)
                    {
                        sendMsg(clfd, "DELETE");
                    }
                    else
                    {
                        stat(absFilePath, &finfo);
                        AppendFile(&listUpdated, filePathToSend, -1, finfo.st_mtime, entry->d_type);
                        sendMsg(clfd, "CLOSE");
                        fclose(fp);
                    }
                }
            }
            else
            {
                free(msgPtr);
                stat(absFilePath, &finfo);
                sendLong(clfd, finfo.st_mtime);
                fp = fopen(absFilePath, "rb");

                while (fp != NULL && (readBytes = fread(readBuff, 1, sizeof(readBuff), fp)) > 0)
                {
                    sendSizedMsg(clfd, readBuff, readBytes);
                }

                sendMsg(clfd, "FILE SENT3");
                if (fp == NULL)
                {
                    sendMsg(clfd, "DELETE");
                }
                else
                {
                    AppendFile(&listUpdated, filePathToSend, -1, finfo.st_mtime, entry->d_type);
                    sendMsg(clfd, "CLOSE");
                    fclose(fp);
                }
            }
        }
    }
}
// compare server's files with the client
void *UpdateExistingFiles(char dirPath[], char pathToSend[], int clientFd, int clientId)
{
    FILE *fp;
    short readBytes;
    char absPath[2000], relPath[1024], msg[1024], *msgPtr;
    struct stat finfo;
    long file_mTime;

    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(dirPath)))
        return NULL;

    // check the files that exist in server exist in client as well
    while ((entry = readdir(dir)) != NULL)
    {
        // exclude current and parent directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        else if (entry->d_type == 4)
        {
            char subDirPath[1024];
            // store the path to traverse subdirectory
            snprintf(subDirPath, sizeof(subDirPath), "%s/%s", dirPath, entry->d_name);
            if (strcmp(pathToSend, "/") == 0)
                snprintf(relPath, sizeof(relPath), "%s", entry->d_name);
            else
                snprintf(relPath, sizeof(relPath), "%s/%s", pathToSend, entry->d_name);

            // send file name to client
            sendMsg(clientFd, relPath);
            sendLong(clientFd, entry->d_type);
            long cl_mTime = rcvLong(clientFd);

            pthread_mutex_lock(&Mutex_fileUpdated);
            if (cl_mTime == 1)
            {
                File *file = SearchInFiles(&listUpdated, relPath);
                if (file != NULL)
                {
                    file->deleted = true;
                    file->mTime = __LONG_MAX__;
                }
                else
                {
                    AppendFile(&listUpdated, relPath, -1, __LONG_MAX__, entry->d_type);
                    file = SearchInFiles(&listUpdated, relPath);
                    file->deleted = true;
                }
            }
            else
            {
                stat(subDirPath, &finfo);
                AppendFile(&listUpdated, relPath, -1, finfo.st_mtime, entry->d_type);
            }
            pthread_mutex_unlock(&Mutex_fileUpdated);

            // recursively traverse the subdirectory
            UpdateExistingFiles(subDirPath, relPath, clientFd, clientId);
        }
        else if (entry->d_type == 8)
        {
            if (strcmp(pathToSend, "/") == 0)
                snprintf(relPath, sizeof(relPath), "%s", entry->d_name);
            else
                snprintf(relPath, sizeof(relPath), "%s/%s", pathToSend, entry->d_name);

            // send file name to client
            sendMsg(clientFd, relPath);
            sendLong(clientFd, entry->d_type);
            long cl_mTime = rcvLong(clientFd);

            sprintf(absPath, "%s/%s", SV_DIR, relPath);

            stat(absPath, &finfo);
            long sv_mTime = finfo.st_mtime;
            if (cl_mTime > sv_mTime)
            {
                File *file = NULL;
                pthread_mutex_lock(&Mutex_fileUpdated);
                if ((file = SearchInFiles(&listUpdated, relPath)) != NULL)
                {
                    if (file->mTime < cl_mTime)
                    {
                        file->mTime = cl_mTime;
                        file->ownerId = clientId;
                    }
                }
                else
                {
                    AppendFile(&listUpdated, relPath, clientId, cl_mTime, entry->d_type);
                }
                pthread_mutex_unlock(&Mutex_fileUpdated);
            }
            else if (sv_mTime >= cl_mTime && cl_mTime != 1)
            {
                File *file = NULL;
                pthread_mutex_lock(&Mutex_fileUpdated);
                if ((file = SearchInFiles(&listUpdated, relPath)) != NULL)
                {
                    if (file->mTime <= sv_mTime)
                    {
                        file->mTime = sv_mTime;
                        file->ownerId = -1;
                    }
                }
                else
                {
                    AppendFile(&listUpdated, relPath, -1, sv_mTime, entry->d_type);
                }
                pthread_mutex_unlock(&Mutex_fileUpdated);
            }
            else
            {
                pthread_mutex_lock(&Mutex_fileUpdated);
                File *file = SearchInFiles(&listUpdated, relPath);
                if (file != NULL)
                {
                    file->deleted = true;
                    file->mTime = __LONG_MAX__;
                }
                else
                {
                    AppendFile(&listUpdated, relPath, -1, __LONG_MAX__, entry->d_type);
                    file = SearchInFiles(&listUpdated, relPath);
                    file->deleted = true;
                }
                pthread_mutex_unlock(&Mutex_fileUpdated);
            }
        }
    }
    closedir(dir);
}
// determine files that client has but server does not have
void *UpdateInexistingFiles(int clientFd, int clientId)
{
    char *msgPtr, relFilePath[MAX];
    while (1)
    {
        msgPtr = receiveMsg(clientFd);
        strcpy(relFilePath, msgPtr);
        free(msgPtr);

        if (strcmp(relFilePath, "DONE4") == 0)
        {
            break;
        }

        long fType = rcvLong(clientFd);
        long cl_mTime = rcvLong(clientFd);
        pthread_mutex_lock(&Mutex_fileUpdated);

        AppendFile(&listUpdated, relFilePath, clientId, cl_mTime, fType);

        pthread_mutex_lock(&Mutex_fileLastSync);
        File *file;
        // if file existed in last sync but doesnt exist right now, then it is deleted
        if ((file = SearchInFiles(&listLastSync, relFilePath)) != NULL)
        {

            file = SearchInFiles(&listUpdated, relFilePath);
            file->deleted = true;
            file->mTime = __LONG_MAX__;
        }
        pthread_mutex_unlock(&Mutex_fileLastSync);
        pthread_mutex_unlock(&Mutex_fileUpdated);
    }
}
// update server by getting files from client
void *UpdateServer(int clientId, int clientFd)
{
    FILE *fp;
    char *msgPtr, buff[MAX], readBuff[MAX], absFilePath[MAX];
    short readBytes;
    while (1)
    {
        pthread_mutex_lock(&Mutex_fileUpdated);
        File *file = SearchInFilesByOwnerId(&listUpdated, clientId);
        pthread_mutex_unlock(&Mutex_fileUpdated);

        if (file == NULL)
        {
            sendMsg(clientFd, "DONE5");
            break;
        }
        sprintf(absFilePath, "%s/%s", SV_DIR, file->path);
        if (file->deleted)
        {
            file->ownerId = -1;
            continue;
        }

        // send filename to client (relative path)
        sendMsg(clientFd, file->path);

        sendLong(clientFd, file->type);

        file->ownerId *= -1;
        if (file->type == 4)
        {
            CreateDirectory(SV_DIR, file->path);
            continue;
        }

        // receive modification time
        long cl_mTime = rcvLong(clientFd);
        fp = fopen(absFilePath, "wb");
        // read the file
        while (1)
        {
            // store as byte and string
            msgPtr = receiveSizedMsg(clientFd, &readBytes);
            strcpy(buff, msgPtr);
            memcpy(readBuff, msgPtr, readBytes);
            free(msgPtr);
            if (strcmp(buff, "FILE SENT5") == 0)
                break;

            fwrite(readBuff, 1, readBytes, fp);
        }
        msgPtr = receiveMsg(clientFd);
        fclose(fp);
        if (strcmp(msgPtr, "DELETE") == 0)
        {
            file->deleted = true;
        }
        else
        {
            ChangeFileMTime(absFilePath, cl_mTime);
        }
        free(msgPtr);
    }
}
// update the client by sending files from the server
void *UpdateClient(int clientId, int clientFd)
{
    FILE *fp;
    struct stat finfo;
    File *file;
    short readBytes;
    char absFilePath[MAX], relFilePath[MAX], readBuff[MAX], buff[MAX];

    UnmarkAll(&listUpdated);

    while (1)
    {
        file = GetUnmarkedFile(&listUpdated);
        if (file == NULL)
        {
            sendMsg(clientFd, "DONE6");
            break;
        }
        else
        {
            file->marked = true;
        }
        // send filename to client
        sendMsg(clientFd, file->path);
        sendLong(clientFd, file->type);
        sprintf(absFilePath, "%s/%s", SV_DIR, file->path);

        if (file->type == 4)
        {
            if (file->deleted)
            {

                sendMsg(clientFd, "DELETE");
            }
            else
            {
                sendMsg(clientFd, "WRITE");
            }

            continue;
        }
        else if (file->type == 8)
        {
            if (file->deleted)
            {
                sendMsg(clientFd, "DELETE");
                continue;
            }
            else
            {
                sendMsg(clientFd, "WRITE");
            }

            sendLong(clientFd, file->mTime);
            fp = fopen(absFilePath, "rb");

            while (fp != NULL && (readBytes = fread(buff, 1, sizeof(buff), fp)) > 0)
            {
                sendSizedMsg(clientFd, buff, readBytes);
            }
            sendMsg(clientFd, "FILE SENT6");
            // if file is deleted after the synchornization had started, tell client to delete it
            if (fp == NULL)
            {
                printf("file null2\n");
                sendMsg(clientFd, "DELETE");
                pthread_mutex_lock(&Mutex_fileUpdated);
                File *file = SearchInFiles(&listUpdated, file->path);
                if (file != NULL)
                    file->deleted = true;
                pthread_mutex_lock(&Mutex_fileUpdated);
                // file->deleted = true;
            }
            else
            {
                sendMsg(clientFd, "CLOSE");
                fclose(fp);
            }
        }
    }
    // CleanFiles(&updatedListCopy);
}
#endif