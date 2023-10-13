#ifndef CLFUNCTIONS_H
#define CLFUNCTIONS_H

#include <arpa/inet.h> // inet_addr()
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h> // read(), write(), close()
#include <dirent.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include "commonfuncs.h"
#include <errno.h>
#include "filefuncs.h"
int PORT;
#define SA struct sockaddr

void SyncAtConnection(int svFd, char cl_dir[256], File **listCurrentReg, File **listLastSync)
{
    // enqueue client files
    ListFiles(cl_dir, "/", listCurrentReg);

    struct stat finfo;
    char fileName[256], filePath[1300], *msgPtr, readBuff[MAX], wrbuff[MAX], rdbuff[MAX];
    FILE *fp;
    short readBytes;
    long fileType;

    while (1)
    {
        // file name will be received
        msgPtr = receiveMsg(svFd);
        strcpy(fileName, msgPtr);
        free(msgPtr);
        if (strcmp(fileName, "DONE1") == 0)
        {
            break;
        }

        // file type will be received (directory or regular)
        fileType = rcvLong(svFd);
        sprintf(filePath, "%s/%s", cl_dir, fileName);
        File *file;
        if (fileType == 4)
        {
            AppendFile(listLastSync, fileName, 0, 0, 4);

            if ((file = SearchInFiles(listCurrentReg, fileName)) != NULL)
            {
                file->marked = true;
            }
            else
            {
                CreateDirectory(cl_dir, fileName);
            }
        }
        else if (fileType == 8)
        {
            if ((file = SearchInFiles(listCurrentReg, fileName)) != NULL)
            {
                file->marked = true;
                // inform server about file existance
                sendMsg(svFd, "file exists");

                // receive file last modification time in server
                long sv_filetime = rcvLong(svFd);
                stat(filePath, &finfo);

                if (finfo.st_mtime > sv_filetime)
                {
                    // inform server that you will send
                    sendMsg(svFd, "client sending");
                    // send last modification time
                    sendLong(svFd, finfo.st_mtime);
                    fp = fopen(filePath, "rb");
                    while (fp != NULL && (readBytes = fread(readBuff, 1, sizeof(readBuff), fp)) > 0)
                    {
                        sendSizedMsg(svFd, readBuff, readBytes);
                    }
                    sendMsg(svFd, "FILE SENT1");
                    if (fp == NULL)
                    {
                        sendMsg(svFd, "DELETE");
                    }
                    else
                    {
                        AppendFile(listLastSync, fileName, 0, 0, 8);
                        sendMsg(svFd, "CLOSE");
                        fclose(fp);
                    }
                }
                else
                {
                    // inform server that you will receive
                    sendMsg(svFd, "client receiving");
                    fp = fopen(filePath, "wb");
                    while (1)
                    {
                        // store as byte and string
                        msgPtr = receiveSizedMsg(svFd, &readBytes);
                        strcpy(rdbuff, msgPtr);
                        memcpy(readBuff, msgPtr, readBytes);
                        free(msgPtr);
                        if (strcmp(rdbuff, "FILE SENT2") == 0)
                        {
                            break;
                        }
                        fwrite(readBuff, 1, readBytes, fp);
                    }
                    msgPtr = receiveMsg(svFd);
                    fclose(fp);
                    if (strcmp(msgPtr, "DELETE") == 0)
                        remove(filePath);
                    else
                    {
                        AppendFile(listLastSync, fileName, 0, 0, 8);
                        ChangeFileMTime(filePath, sv_filetime);
                    }
                    free(msgPtr);
                }
            }
            else
            {
                sendMsg(svFd, "file not found");

                fp = fopen(filePath, "wb");
                // receive file last modification time
                long int mTime = rcvLong(svFd);
                while (1)
                {
                    // store as byte and string
                    msgPtr = receiveSizedMsg(svFd, &readBytes);
                    strcpy(rdbuff, msgPtr);
                    memcpy(readBuff, msgPtr, readBytes);
                    free(msgPtr);
                    if (strcmp(rdbuff, "FILE SENT3") == 0)
                    {
                        break;
                    }
                    fwrite(readBuff, 1, readBytes, fp);
                }
                msgPtr = receiveMsg(svFd);
                fclose(fp);
                if (strcmp(msgPtr, "DELETE") == 0)
                {
                    remove(filePath);
                }
                else
                {
                    ChangeFileMTime(filePath, mTime);
                    AppendFile(listLastSync, fileName, 0, 0, 8);
                }
                free(msgPtr);
            }
        }
    }
    File *file;
    while ((file = GetUnmarkedFile(listCurrentReg)) != NULL)
    {
        file->marked = true;
        strcpy(fileName, file->path);
        long fType = file->type;

        sendMsg(svFd, fileName);
        sendLong(svFd, fType);
        // directory sent
        if (fType == 4)
        {
            AppendFile(listLastSync, fileName, 0, 0, fType);
            continue;
        }

        sprintf(filePath, "%s/%s", cl_dir, fileName);
        fp = fopen(filePath, "rb");
        stat(filePath, &finfo);
        sendLong(svFd, finfo.st_mtime);
        char readBuff[MAX];
        while (fp != NULL && (readBytes = fread(readBuff, 1, sizeof(readBuff), fp)) > 0)
        {
            sendSizedMsg(svFd, readBuff, readBytes);
        }
        sendMsg(svFd, "FILE SENT4");
        if (fp == NULL)
        {
            sendMsg(svFd, "DELETE");
        }
        else
        {
            AppendFile(listLastSync, fileName, 0, 0, 8);
            sendMsg(svFd, "CLOSE");
            fclose(fp);
        }
    }
    sendMsg(svFd, "DONE2");
}

void Synchronize(int svFd, char cl_dir[256])
{
    File *listLastSync = NULL;
    File *listCurrentReg = NULL;
    CreateRootDir(&listLastSync, cl_dir);
    CreateRootDir(&listCurrentReg, cl_dir);

    struct stat finfo;
    char relFilePath[256];
    char absFilePath[1300];
    short readBytes;
    char *msgPtr;
    FILE *fp;
    char readBuff[MAX];
    char wrbuff[MAX] = "";
    char rdbuff[MAX] = "";

    SyncAtConnection(svFd, cl_dir, &listCurrentReg, &listLastSync);
    while (1)
    {
        CleanFiles(&listCurrentReg);
        CreateRootDir(&listCurrentReg, cl_dir);
        ListFiles(cl_dir, "/", &listCurrentReg);

        while (1)
        {
            msgPtr = receiveMsg(svFd);
            strcpy(relFilePath, msgPtr);
            free(msgPtr);
            if (strcmp(relFilePath, "DONE3") == 0)
                break;

            sprintf(absFilePath, "%s/%s", cl_dir, relFilePath);

            long fType = rcvLong(svFd);

            if (stat(absFilePath, &finfo) == 0)
            {
                sendLong(svFd, finfo.st_mtime);
                File *file = SearchInFiles(&listCurrentReg, relFilePath);
                file->marked = true;
                // DeleteGivenFile(&listCurrentReg, relFilePath);
            }
            else if (SearchInFiles(&listLastSync, relFilePath) == NULL)
            {
                long mTime = 0;
                sendLong(svFd, mTime);
            }
            else
            {
                long mTime = 1;
                sendLong(svFd, mTime);
            }
        }
        File *file;
        while ((file = GetUnmarkedFile(&listCurrentReg)) != NULL)
        {
            file->marked = true;
            strcpy(relFilePath, file->path);

            sendMsg(svFd, relFilePath);
            sendLong(svFd, file->type);

            sprintf(absFilePath, "%s/%s", cl_dir, relFilePath);
            stat(absFilePath, &finfo);
            sendLong(svFd, finfo.st_mtime);
        }
        sendMsg(svFd, "DONE4");

        CleanFiles(&listLastSync);
        CreateRootDir(&listLastSync, cl_dir);
        // if you have the most up to date or newly created file, then upload it to the server
        while (1)
        {
            msgPtr = receiveMsg(svFd);
            strcpy(relFilePath, msgPtr);
            free(msgPtr);

            if (strcmp(relFilePath, "DONE5") == 0)
            {
                break;
            }
            long fType = rcvLong(svFd);
            if (fType == 4)
            {
                AppendFile(&listLastSync, relFilePath, 0, 0, 4);
            }
            else if (fType == 8)
            {
                sprintf(absFilePath, "%s/%s", cl_dir, relFilePath);

                stat(absFilePath, &finfo);
                sendLong(svFd, finfo.st_mtime);

                fp = fopen(absFilePath, "rb");
                while (fp != NULL && (readBytes = fread(readBuff, 1, sizeof(readBuff), fp)) > 0)
                {
                    sendSizedMsg(svFd, readBuff, readBytes);
                }

                sendMsg(svFd, "FILE SENT5");
                if (fp == NULL)
                {
                    sendMsg(svFd, "DELETE");
                }
                else
                {
                    AppendFile(&listLastSync, relFilePath, 0, 0, 8);
                    sendMsg(svFd, "CLOSE");
                    fclose(fp);
                }
            }
        }

        // get the most up to date files from server
        while (1)
        {
            // receive file name
            msgPtr = receiveMsg(svFd);
            strcpy(relFilePath, msgPtr);
            free(msgPtr);
            if (strcmp(relFilePath, "DONE6") == 0)
                break;

            long fType = rcvLong(svFd);

            sprintf(absFilePath, "%s/%s", cl_dir, relFilePath);

            if (fType == 4)
            {
                msgPtr = receiveMsg(svFd);
                strcpy(rdbuff, msgPtr);
                free(msgPtr);
                if (strcmp("DELETE", rdbuff) == 0)
                {
                    RemoveDirectory(absFilePath);
                    continue;
                }
                else
                {
                    AppendFile(&listLastSync, relFilePath, 0, 0, 4);
                    // printf("create dir:%s\n",relFilePath);
                    CreateDirectory(cl_dir, relFilePath);
                }
                continue;
            }
            else if (fType == 8)
            {
                msgPtr = receiveMsg(svFd);
                strcpy(rdbuff, msgPtr);
                free(msgPtr);
                if (strcmp("DELETE", rdbuff) == 0)
                {
                    remove(absFilePath);
                    continue;
                }
                else
                {
                    // receive file modification time
                    long sv_mTime = rcvLong(svFd);
                    fp = fopen(absFilePath, "wb");
                    // read the file
                    while (1)
                    {
                        // store as byte and string
                        msgPtr = receiveSizedMsg(svFd, &readBytes);
                        strcpy(rdbuff, msgPtr);
                        memcpy(readBuff, msgPtr, readBytes);
                        free(msgPtr);
                        if (strcmp(rdbuff, "FILE SENT6") == 0)
                            break;
                        fwrite(readBuff, 1, readBytes, fp);
                    }
                    msgPtr = receiveMsg(svFd);
                    fclose(fp);

                    if (strcmp(msgPtr, "DELETE") == 0)
                    {
                        remove(absFilePath);
                    }
                    else
                    {
                        AppendFile(&listLastSync, relFilePath, 0, 0, 8);
                        ChangeFileMTime(absFilePath, sv_mTime);
                    }
                    free(msgPtr);
                }
            }
        }
    }
}
#endif