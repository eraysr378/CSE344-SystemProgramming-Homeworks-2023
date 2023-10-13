#define _POSIX_C_SOURCE 200112L
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include "mytypes.h"
#include "myfunctions.h"

int main(int argc, char const *argv[])
{
    if (argc != 5)
    {
        printf("Usage:./main <bufferSize> <numOfConsumers> <sourceDir> <destinationDir> \n");
        exit(0);
    }
    // store source and destination directories to send them to producer thread
    Directories direcs;
    strcpy(direcs.source, argv[3]);
    strcpy(direcs.destination, argv[4]);
    direcs.dirCount = 0;
    BUFFER_SIZE = atoi(argv[1]);
    POOL_SIZE = atoi(argv[2]);

    direcs.bufferSize = BUFFER_SIZE;
    if (POOL_SIZE <= 0)
    {
        printf("Consumer pool size should be greater than 0\n");
        return -1;
    }
    if (BUFFER_SIZE <= 0)
    {
        printf("Buffer size should be greater than 0\n");
        return -1;
    }
    // if source directory does not exist, terminate
    if (mkdir(direcs.source, S_IRWXU | S_IRWXG | S_IRWXO) == 0)
    {
        printf("source directory does not exist\n");
        rmdir(direcs.source);
        return -1;
    }

    // if destination directory does not exist, create it
    if (mkdir(direcs.destination, S_IRWXU | S_IRWXG | S_IRWXO) != 0)
    {
        if (errno != 17)
        {
            printf("**error: directory cannot be created %s, terminating...\n", direcs.destination);
            return -1;
        }
        else
        {
            // if destination directory already exists, then copy the source directory itself as well.
            char sourceDirName[256];
            char *token;
            strcpy(sourceDirName, direcs.source);
            token = strtok(sourceDirName, "/");
            while (token != NULL)
            {
                strcpy(sourceDirName, token);
                token = strtok(NULL, "/");
            }
            char newDirec[512];
            sprintf(newDirec, "%s/%s", direcs.destination, sourceDirName);
            mkdir(newDirec, S_IRWXU | S_IRWXG | S_IRWXO);
            strcpy(direcs.destination, newDirec);
        }
    }else{
        printf("directory does not exist\n");
    }

    Buffer = (FileInfoBuffer *)malloc(BUFFER_SIZE * sizeof(FileInfoBuffer));

    pthread_mutex_init(&Mutex_m, NULL);
    pthread_mutex_init(&Mutex_write_stdout, NULL);
    pthread_mutex_init(&Mutex_open_file, NULL);

    pthread_cond_init(&Cond_empty, NULL);
    pthread_cond_init(&Cond_full, NULL);
    pthread_cond_init(&Cond_fd_count, NULL);

    // signal handling
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &MySignalHandler;
    sigaction(SIGINT, &sa, NULL);

    // initialize buffer's sourceFd values as -1 that means it is empty
    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        Buffer[i].sourceFd = -1;
    }

    ConsumerThreads = malloc(sizeof(pthread_t) * POOL_SIZE);

    struct timeval current_time;
    time_t t_start;
    time_t t_end;
    // get the time before the first thread is created
    gettimeofday(&current_time, NULL);
    t_start = current_time.tv_sec;

    pthread_create(&ProducerThread, NULL, &startProducerThread, &direcs);
    int i;
    for (i = 0; i < POOL_SIZE; i++)
    {
        if (pthread_create(&ConsumerThreads[i], NULL, &startConsumerThread, &direcs) != 0)
        {
            perror("Failed to create the thread");
        }
    }

    for (int i = 0; i < POOL_SIZE; i++)
    {
        if (pthread_join(ConsumerThreads[i], NULL) != 0)
        {
            perror("Failed to join the thread");
        }
    }
    pthread_join(ProducerThread, NULL);
    // get the time after all threads are joined
    gettimeofday(&current_time, NULL);
    t_end = current_time.tv_sec;
    printf("Directory copied in %ld seconds.\n", (t_end - t_start));

    // status of succeessful
    printf("Regular Files: %d succeed, %d failed.\n", FilesSucceed, FilesFailed);
    printf("FIFO Files: %d succeed, %d failed.\n", FifoSucceed, FifoFailed);
    printf("Directories: %d succeed, %d failed.\n", DirsSucceed, DirsFailed);
    printf("Total of %d bytes have been copied.\n", TotalByteCopied);
    pthread_mutex_destroy(&Mutex_write_stdout);
    pthread_mutex_destroy(&Mutex_m);
    pthread_mutex_destroy(&Mutex_open_file);
    pthread_cond_destroy(&Cond_empty);
    pthread_cond_destroy(&Cond_full);
    pthread_cond_destroy(&Cond_fd_count);
    if (Buffer != NULL)
        free(Buffer);
    if (ConsumerThreads != NULL)
        free(ConsumerThreads);
    return 0;
}
