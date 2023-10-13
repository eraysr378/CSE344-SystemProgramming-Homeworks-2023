#ifndef SVFUNCTIONS_h
#define SVFUNCTIONS_h

#include "recursive.h"

void SyncAtConnection(int *clientFdptr, int *clientIdptr)
{

    struct stat finfo;
    FILE *fp;
    char buff[MAX];
    char *msgPtr;
    char readBuff[MAX];
    char filePath[1300];
    int fd;
    short readBytes;

    // wait for the client
    pthread_mutex_lock(&Mutex_connFd);

    while (FrontOfClientQueue() == -1)
    {
        printf("queue is empty\n");
        pthread_cond_wait(&Cond_fdReady, &Mutex_connFd);
    }
    *clientFdptr = DequeueClient();
    *clientIdptr = ClientIdCount;
    ClientIdCount++;

    int clientFd = *clientFdptr;
    int clientId = *clientIdptr;

    pthread_mutex_unlock(&Mutex_connFd);
    ////
    pthread_mutex_lock(&Mutex_sync_or_connect);
    while (ConnectionSync || SyncStarted)
    {
        printf("%d:connectiong pending\n", clientId);
        pthread_cond_wait(&Cond_sync_or_connect, &Mutex_sync_or_connect);
    }
    printf("%d:connected\n", clientId);

    ConnectionSync = true;

    CleanFiles(&listLastSync);
    CreateRootDir(&listLastSync, SV_DIR);

    // make the client know that there is a thread taking care of them
    sendMsg(clientFd, "get in");

    TransferFilesAtConnection(SV_DIR, "/", clientFd, clientId);
    sendMsg(clientFd, "DONE1");
    // check if there are files in client that are not in server
    while (1)
    {
        char fileName[MAX];
        msgPtr = receiveMsg(clientFd);
        strcpy(fileName, msgPtr);
        free(msgPtr);

        if (strcmp(fileName, "DONE2") == 0)
        {
            break;
        }
        long fType = rcvLong(clientFd);

        sprintf(filePath, "%s/%s", SV_DIR, fileName);
        // directory received, create it
        if (fType == 4)
        {
            CreateDirectory(SV_DIR, fileName);
            AppendFile(&listUpdated, fileName, -1, 0, fType);
            continue;
        }

        long cl_mTime = rcvLong(clientFd);

        fp = fopen(filePath, "wb");
        while (1)
        {
            // store as byte and string
            msgPtr = receiveSizedMsg(clientFd, &readBytes);
            strcpy(buff, msgPtr);
            memcpy(readBuff, msgPtr, readBytes);
            free(msgPtr);
            if (strcmp(buff, "FILE SENT4") == 0)
            {
                break;
            }
            fwrite(readBuff, 1, readBytes, fp);
        }
        msgPtr = receiveMsg(clientFd);
        fclose(fp);
        if (strcmp(msgPtr, "DELETE") == 0)
        {
            remove(filePath);
        }
        else
        {
            ChangeFileMTime(filePath, cl_mTime);
            AppendFile(&listUpdated, fileName, clientId, cl_mTime, fType);
        }
        free(msgPtr);
    }
    printf("Server and client synchronized\n");
    ConnectionSync = false;
    ConnectionRequests--;
    ReadyToSyncCount++;
    pthread_mutex_unlock(&Mutex_sync_or_connect);
    pthread_cond_broadcast(&Cond_sync_or_connect);
}

void *startThread(void *args)
{
    struct stat finfo;
    FILE *fp;
    char buff[MAX], readBuff[MAX], filePath[1300], *msgPtr;
    int clientId, clientFd;
    short readBytes;

    while (1)
    {
        if (listUpdated == NULL)
        {
            CreateRootDir(&listUpdated, SV_DIR);
        }
        SyncAtConnection(&clientFd, &clientId);
        while (1)
        {
            pthread_mutex_lock(&Mutex_sync_or_connect);
            while (ConnectionSync || ConnectionRequests > 0)
            {
                pthread_cond_broadcast(&Cond_sync_or_connect);
                pthread_cond_wait(&Cond_sync_or_connect, &Mutex_sync_or_connect);
            }

            SyncStarted = true;

            pthread_mutex_unlock(&Mutex_sync_or_connect);

            UpdateExistingFiles(SV_DIR, "/", clientFd, clientId);
            sendMsg(clientFd, "DONE3");

            UpdateInexistingFiles(clientFd, clientId);

            // file transfer starting wait for all threads to finish updating list
            pthread_mutex_lock(&Mutex_sync_threads);
            SyncCheckCount++;
            if (SyncCheckCount < ReadyToSyncCount)
            {
                // printf("sync check client:%d,sync:%d\n", ReadyToSyncCount, SyncCheckCount);
                pthread_cond_wait(&Cond_sync_threads, &Mutex_sync_threads);
            }
            if (SyncCheckCount == ReadyToSyncCount)
            {
                SyncCheckCount = 0;
                pthread_cond_broadcast(&Cond_sync_threads);
            }
            pthread_mutex_unlock(&Mutex_sync_threads);

            UpdateServer(clientId, clientFd);

            pthread_mutex_lock(&Mutex_sync_threads);
            SyncClientCount++;
            if (SyncClientCount < ReadyToSyncCount)
            {
                // printf("waiting other threads client:%d,sync:%d\n", ReadyToSyncCount, SyncClientCount);
                pthread_cond_wait(&Cond_sync_threads, &Mutex_sync_threads);
            }
            if (SyncClientCount == ReadyToSyncCount)
            {
                SyncClientCount = 0;
                pthread_cond_broadcast(&Cond_sync_threads);
            }
            pthread_mutex_unlock(&Mutex_sync_threads);

            pthread_mutex_lock(&Mutex_fileUpdated);
            UpdateClient(clientId, clientFd);
            pthread_mutex_unlock(&Mutex_fileUpdated);

            pthread_mutex_lock(&Mutex_sync_threads);
            SyncDoneCount++;
            if (SyncDoneCount < ReadyToSyncCount)
            {
                // printf("sync done check client:%d,sync:%d\n", ReadyToSyncCount, SyncDoneCount);
                pthread_cond_wait(&Cond_sync_threads, &Mutex_sync_threads);
            }
            if (SyncDoneCount == ReadyToSyncCount)
            {
                pthread_mutex_lock(&Mutex_fileUpdated);

                UnmarkAll(&listUpdated);
                File *file;
                while (1)
                {
                    file = GetUnmarkedFile(&listUpdated);
                    if (file == NULL)
                        break;
                    else
                        file->marked = true;

                    if (file->deleted)
                    {
                        char absPath[MAX];
                        sprintf(absPath, "%s/%s", SV_DIR, file->path);

                        if (file->type == 4)
                            RemoveDirectory(absPath);
                        else if (file->type == 8)
                            remove(absPath);

                        DeleteGivenFile(&listUpdated, file->path);
                    }
                }
                // create list of files that exist in last sync
                UnmarkAll(&listUpdated);
                CleanFiles(&listLastSync);
                while (1)
                {
                    file = GetUnmarkedFile(&listUpdated);
                    if (file == NULL)
                        break;
                    file->marked = true;
                    AppendFile(&listLastSync, file->path, file->ownerId, file->mTime, file->type);
                }

                pthread_mutex_unlock(&Mutex_fileUpdated);
                SyncDoneCount = 0;
                pthread_cond_broadcast(&Cond_sync_threads);

                // make the server available to synchronize newly connected clients
                pthread_mutex_lock(&Mutex_sync_or_connect);
                SyncStarted = false;
                pthread_cond_broadcast(&Cond_sync_or_connect);
                pthread_mutex_unlock(&Mutex_sync_or_connect);
            }
            pthread_mutex_unlock(&Mutex_sync_threads);
            sleep(1); // wait for a second
        }
    }
}
#endif