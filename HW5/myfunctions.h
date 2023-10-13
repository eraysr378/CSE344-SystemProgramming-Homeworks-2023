#define _GNU_SOURCE
// valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./main 5 5 src dest
#ifndef MYFUNCTIONS_H
#define MYFUNCTIONS_H
#include "myglobals.h"
#include "mytypes.h"
#include <pthread.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>

// if the file exists in the directory, returns 1 otherwise -1
int SeekFileInGivenDir(char filename[], const char directory[])
{
    DIR *d;
    struct dirent *dir;
    char dirPath[50];
    sprintf(dirPath, "./%s", directory);
    d = opendir(dirPath);
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            // only send files, exclude directories etc.

            if (strcmp(dir->d_name, filename) == 0)
            {
                closedir(d);
                return 1;
            }
        }
    }
    closedir(d);
    return -1;
}
// search an empty place in given FileInfoBuffer array, returns the index on success, otherwise -1
int FindEmptyInBuffer(FileInfoBuffer *buffer, int size)
{

    for (int i = 0; i < size; i++)
    {
        if (buffer[i].sourceFd == -1)
        {
            return i;
        }
    }
    return -1;
}
// search a full place in given FileInfoBuffer array, returns the index on success, otherwise -1
int FindFullInBuffer(FileInfoBuffer *buffer, int size)
{
    for (int i = 0; i < size; i++)
    {
        if (buffer[i].sourceFd != -1)
        {
            return i;
        }
    }
    return -1;
}

void *startConsumerThread(void *args)
{
    // consumer thread should not be effected from the signal, main program will handle
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    Directories *direcs = args;
    int bufferSize = direcs->bufferSize;
    while (1)
    {
        int bufIndex = -1;
        pthread_mutex_lock(&Mutex_m);
        while ((bufIndex = FindFullInBuffer(Buffer, bufferSize)) == -1)
        {
            // producer stopped producing or kill signal received
            if (Done == 1 || KillSignal == 1)
            {
                // don't let others wait
                pthread_cond_signal(&Cond_full);
                pthread_mutex_unlock(&Mutex_m);
                return NULL;
            }
            pthread_cond_wait(&Cond_full, &Mutex_m);
        }
        FileInfoBuffer chosenFile = Buffer[bufIndex];
        Buffer[bufIndex].sourceFd = -1; // make it empty for producer

        pthread_cond_signal(&Cond_empty);
        pthread_mutex_unlock(&Mutex_m);

        char readBuff[1024];
        int readBytes;
        int fileSize = 0;
        while ((readBytes = read(chosenFile.sourceFd, readBuff, sizeof(readBuff))) > 0)
        {
            if (KillSignal == 1)
            {
                close(chosenFile.sourceFd);
                close(chosenFile.destFd);
                return NULL;
            }
            fileSize += readBytes;
            write(chosenFile.destFd, readBuff, readBytes);
        }
        // Protect writing to stdout and totalByte count and file count at the same time no need for another mutex
        pthread_mutex_lock(&Mutex_write_stdout);
        printf("consumer|file:%s has been copied successfully. %d bytes copied.\n", chosenFile.fileName, fileSize);
        TotalByteCopied += fileSize;
        FilesSucceed++;
        close(chosenFile.sourceFd);
        close(chosenFile.destFd);
        pthread_mutex_unlock(&Mutex_write_stdout);

        // consumer is done with the files, decrease the opened file count
        pthread_mutex_lock(&Mutex_open_file);
        OpenedFileCount -= 2;
        pthread_cond_signal(&Cond_fd_count); // signal for producer to produce
        pthread_mutex_unlock(&Mutex_open_file);
    }
}

void *startProducerThread(void *args)
{
    // producer thread should not be effected from the signal, main program will handle
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    Directories *direcs = args;
    int bufferSize = direcs->bufferSize;
    int bufIndex = 0;
    int fd_src, fd_dest;
    char srcFilePath[512];
    char destFilePath[512];
    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(direcs->source)))
        return NULL;

    while ((entry = readdir(dir)) != NULL)
    {
        // printf("%s,%d\n", entry->d_name, entry->d_type);
        if (KillSignal == 1)
        {
            closedir(dir);
            return NULL;
        }
        // exclude current and parent directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        // Another directory (subdirectory) found in the directory
        if (entry->d_type == 4)
        {
            char srcpath[1024];
            char destpath[1024];
            // store the path to traverse subdirectory
            snprintf(srcpath, sizeof(srcpath), "%s/%s", direcs->source, entry->d_name);
            // store the path to create the subdirectory in destination directory
            snprintf(destpath, sizeof(destpath), "%s/%s", direcs->destination, entry->d_name);
            pthread_mutex_lock(&Mutex_write_stdout);
            // create the subdirectory in destination
            if (mkdir(destpath, S_IRWXU | S_IRWXG | S_IRWXO) == -1)
            {
                if (errno != 17)
                {
                    printf("**error: directory cannot be created %s\n", destpath);
                    DirsFailed++;
                    pthread_mutex_unlock(&Mutex_write_stdout);
                    continue;
                }
            }
            printf("Directory %s created successfully.\n", destpath);
            pthread_mutex_unlock(&Mutex_write_stdout);
            DirsSucceed++;
            // copy the values
            Directories newDirecs;
            newDirecs.bufferSize = direcs->bufferSize;
            strcpy(newDirecs.source, srcpath);
            strcpy(newDirecs.destination, destpath);
            newDirecs.dirCount = direcs->dirCount + 1; // increase the count to understand that subdirectory is being traversed, if count is 0 then it is main directory.
            // recursively traverse the subdirectory
            startProducerThread(&newDirecs);
            continue;
        }
        // fifo file found in directory, fifo causes infinity loop when copying byte to byte, so creating a fifo in destination directory is reasonable solution.
        else if (entry->d_type == 1)
        {
            char fifoPath[1024];
            snprintf(fifoPath, sizeof(fifoPath), "%s/%s", direcs->destination, entry->d_name);

            pthread_mutex_lock(&Mutex_write_stdout);
            if (mkfifo(fifoPath, S_IRUSR | S_IWUSR | S_IWGRP) == -1 && errno != EEXIST)
            {
                printf("failed: mkfifo: %s\n", fifoPath);
                FifoFailed++;
            }
            else
            {
                printf("FIFO file:%s has been copied successfully.\n", entry->d_name);
                FifoSucceed++;
            }
            pthread_mutex_unlock(&Mutex_write_stdout);

            continue;
        }
        else
        {
            sprintf(srcFilePath, "%s/%s", direcs->source, entry->d_name);
            sprintf(destFilePath, "%s/%s", direcs->destination, entry->d_name);
        }

        // if no more  fd can be opened, then wait for a consumer to finish its job.
        pthread_mutex_lock(&Mutex_open_file);
        while (OpenedFileCount > MAX_OPEN_FILE_COUNT)
        {
            pthread_cond_wait(&Cond_fd_count, &Mutex_open_file);
        }
        pthread_mutex_unlock(&Mutex_open_file);
        if (KillSignal == 1)
        {
            closedir(dir);
            return NULL;
        }

        fd_src = open(srcFilePath, O_RDONLY, 0666);
        // if file exists in the destination directory, truncate the file
        if (SeekFileInGivenDir(entry->d_name, direcs->destination) == 1)
        {
            fd_dest = open(destFilePath, O_WRONLY | O_TRUNC, 0666);
        }
        // otherwise create the file and write to it
        else
        {
            fd_dest = open(destFilePath, O_WRONLY | O_CREAT, 0666);
        }
        // if an error occured, close both of the files
        if (fd_dest == -1 || fd_src == -1)
        {
            pthread_mutex_lock(&Mutex_write_stdout);
            printf("File %s couldn't open!\n", entry->d_name);
            pthread_mutex_unlock(&Mutex_write_stdout);

            close(fd_dest);
            close(fd_src);
            FilesFailed++;
            continue;
        }
        // 2 more files are opened, increase the count
        pthread_mutex_lock(&Mutex_open_file);
        OpenedFileCount += 2;
        pthread_mutex_unlock(&Mutex_open_file);

        pthread_mutex_lock(&Mutex_m);
        // if there is no empty slot in the buffer, keep waiting till an item is consumed
        while ((bufIndex = FindEmptyInBuffer(Buffer, bufferSize)) == -1)
        {
            pthread_cond_wait(&Cond_empty, &Mutex_m);
        }
        if (KillSignal == 1)
        {
            pthread_cond_signal(&Cond_full);
            pthread_mutex_unlock(&Mutex_m);

            if (fd_dest > 0)
                close(fd_dest);
            if (fd_src > 0)
                close(fd_src);

            closedir(dir);
            return NULL;
        }
        // place the current file informations in the buffer.
        strcpy(Buffer[bufIndex].fileName, entry->d_name);
        Buffer[bufIndex].destFd = fd_dest;
        Buffer[bufIndex].sourceFd = fd_src;

        pthread_cond_signal(&Cond_full);
        pthread_mutex_unlock(&Mutex_m);
    }

    // when all the sub directories are traversed, main directory will be traversed and done flag will be raised at the end
    if (direcs->dirCount == 0)
    {
        // protect done flag because consumer may get effected badly
        pthread_mutex_lock(&Mutex_m);
        Done = 1;
        // awake waiting threads
        pthread_cond_broadcast(&Cond_full);

        // also protect print to stdout as it is requested in pdf
        pthread_mutex_lock(&Mutex_write_stdout);
        printf("Producer done.\n");

        pthread_mutex_unlock(&Mutex_write_stdout);
        pthread_mutex_unlock(&Mutex_m);
    }
    closedir(dir);

    return NULL;
}

void MySignalHandler(int signo)
{
    switch (signo)
    {
    case SIGINT:
        KillSignal = 1; // killSİgnal raised, threads will understand that they should finish their execution
        printf("SIGINT received, terminating...\n");
        // clear the buffer and close all opened file descriptors
        if (Buffer != NULL)
        {
            for (int i = 0; i < BUFFER_SIZE; i++)
            {
                if (Buffer[i].sourceFd > 0)
                    close(Buffer[i].sourceFd);

                Buffer[i].sourceFd = -1;

                if (Buffer[i].destFd > 0)
                    close(Buffer[i].destFd);
            }
        }
        OpenedFileCount = 0;

        // make sure that threads are not blocked
        pthread_cond_broadcast(&Cond_empty);
        pthread_cond_broadcast(&Cond_full);
        pthread_cond_signal(&Cond_fd_count);
        return;
        break;

    default:
        break;
    }
}

#endif