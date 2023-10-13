#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#define _POSIX_C_SOURCE 200112L
// base64 /dev/urandom | head -c 10000000 > file.txt
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <semaphore.h>
#include <dirent.h>
#include <fcntl.h>
#include "pidqueue.h"
#include "taskqueue.h"
#include "mytypes.h"
#include "myconst.h"
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
            if (dir->d_type == 8)
            {
                if (strcmp(dir->d_name, filename) == 0)
                {
                    closedir(d);
                    return 1;
                }
            }
        }
    }
    closedir(d);
    return -1;
}
int SeekFileInCurrentDir(char filename[])
{
    DIR *d;
    struct dirent *dir;
    d = opendir(".");
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            // only send files, exclude directories etc.
            if (dir->d_type == 8)
            {
                if (strcmp(dir->d_name, filename) == 0)
                {
                    closedir(d);
                    return 1;
                }
            }
        }
    }
    closedir(d);
    return -1;
}
// if file exists returns true, otherwise false
bool IsFileCreated(char *filename)
{
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}
int GetFileWRCount(int fd)
{
    struct stat sb;
    char *addr = NULL;
    if (fstat(fd, &sb) == -1)
    {
        printf("wr fstat error\n");
        exit(-2);
    }
    // there is nothing to read
    if (sb.st_size <= 0)
    {
        return 0;
    }
    addr = (char *)mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
    int rval = atoi(addr);
    if (addr != NULL)
    {
        munmap(addr, sb.st_size);
    }
    return rval;
}
// changes reader/writer count in the given shared memory segment fd
int ChangeFileWRCount(int fd, char semName[], int val)
{
    sem_t *wr_count_changer = sem_open(semName, O_CREAT, 0600, 1);
    sem_wait(wr_count_changer);

    int newCount = GetFileWRCount(fd) + val;
    char count[5];
    sprintf(count, "%d", newCount);

    int len = strlen(count);
    if (ftruncate(fd, len) == -1)
    {
        printf("wr ftruncate failed\n");
    }
    char *addr = (char *)mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    memcpy(addr, count, len);
    sem_post(wr_count_changer);
    sem_close(wr_count_changer);
    return atoi(addr);
}

void AppendToFile(const char *directory, char filename[], char message[])
{
    char shm_name[110] = "";
    char wmutex[110] = "";
    char readTry[110] = "";
    char rsc[110] = "";
    FILE *fptr;
    char path[100];
    char count_sem_name[110];

    sprintf(path, "%s/%s", directory, filename);
    sprintf(count_sem_name, "%s_%s_count", directory, filename);
    sprintf(wmutex, "%s_%s_wmutex", directory, filename);
    sprintf(readTry, "%s_%s_readTry", directory, filename);
    sprintf(rsc, "%s_%s_rsc", directory, filename);
    // create semaphores that are only for this file
    // so if another process try to read or write, synchronization will be achieved

    sem_t *sem_wmutex = sem_open(wmutex, O_CREAT, 0600, 1);
    sem_t *sem_readTry = sem_open(readTry, O_CREAT, 0600, 1);
    sem_t *sem_rsc = sem_open(rsc, O_CREAT, 0600, 1);

    sem_wait(sem_wmutex);
    sprintf(shm_name, "%s_%s_shm_write", directory, filename);
    int fd_write = shm_open(shm_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    int writerCount = GetFileWRCount(fd_write) + 1;
    ChangeFileWRCount(fd_write, count_sem_name, 1);
    printf(" writer count:%d\n", writerCount);
    if (writerCount == 1)
    {
        sem_wait(sem_readTry);
    }
    sem_post(sem_wmutex);
    sem_wait(sem_rsc);
    if (IsFileCreated(path))
    {
        fptr = fopen(path, "ab"); // if file already exists, then append to it
    }
    else
    {
        fptr = fopen(path, "wb"); // otherwise create the file
    }

    fprintf(fptr, "%s", message);
    fclose(fptr);

    sem_post(sem_rsc);
    sem_wait(sem_wmutex);
    // writer done, decrease the writer count on this file
    writerCount = ChangeFileWRCount(fd_write, count_sem_name, -1);
    printf("writer done count: %d", writerCount);

    if (writerCount == 0)
    {
        sem_post(sem_readTry);
    }
    sem_post(sem_wmutex);
    sem_close(sem_readTry);
    sem_close(sem_wmutex);
    sem_close(sem_rsc);
}
void WriteToLine(const char *directory, char filename[], int lineNum, char message[], int clientPid)
{
    if (lineNum <= 0)
    {
        return;
    }
    char shm_name[110] = "";
    char wmutex[110] = "";
    char readTry[110] = "";
    char rsc[110] = "";
    char count_sem_name[110];

    sprintf(count_sem_name, "%s_%s_count", directory, filename);
    sprintf(wmutex, "%s_%s_wmutex", directory, filename);
    sprintf(readTry, "%s_%s_readTry", directory, filename);
    sprintf(rsc, "%s_%s_rsc", directory, filename);
    // create semaphores that are only for this file
    // so if another process try to read or write, synchronization will be achieved

    sem_t *sem_wmutex = sem_open(wmutex, O_CREAT, 0600, 1);
    sem_t *sem_readTry = sem_open(readTry, O_CREAT, 0600, 1);
    sem_t *sem_rsc = sem_open(rsc, O_CREAT, 0600, 1);

    sem_wait(sem_wmutex);
    sprintf(shm_name, "%s_%s_shm_write", directory, filename);
    int fd_write = shm_open(shm_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    int writerCount = GetFileWRCount(fd_write) + 1;
    ChangeFileWRCount(fd_write, count_sem_name, 1);
    // printf("args:%d writer count:%d\n", argumentNum, writerCount);
    if (writerCount == 1)
    {
        sem_wait(sem_readTry);
    }
    sem_post(sem_wmutex);
    sem_wait(sem_rsc);

    char str[MESSAGE_LEN];
    char tempFilePath[100] = "";
    char filePath[100] = "";
    sprintf(tempFilePath, "%s/__temp_file_client_pid_%d", directory, clientPid);
    sprintf(filePath, "%s/%s", directory, filename);
    FILE *fp_new = fopen(tempFilePath, "wb");
    FILE *fp_old = fopen(filePath, "rb");
    int linectr = 0;
    int written = -1;
    char c;
    char buffer[512];
    while ((fgets(buffer, MAX_CHAR_IN_LINE, fp_old)) != NULL)
    {
        linectr++;
        /* If current line is line to replace */
        if (linectr == lineNum)
        {
            fputs(message, fp_new);
            fputc('\n', fp_new);

            written = 1;
        }
        else
            fputs(buffer, fp_new);
    }
    if (written == -1)
    {
        linectr++;
        while (linectr < lineNum)
        {
            fputc('\n', fp_new);
            linectr++;
        }
        fputs(message, fp_new);
        fputc('\n', fp_new);
    }

    remove(filePath);
    rename(tempFilePath, filePath);
    fclose(fp_new);
    fclose(fp_old);

    sem_post(sem_rsc);
    sem_wait(sem_wmutex);
    // writer done, decrease the writer count on this file
    writerCount = ChangeFileWRCount(fd_write, count_sem_name, -1);
    // printf("writer done count: %d", writerCount);

    if (writerCount == 0)
    {
        sem_post(sem_readTry);
    }
    sem_post(sem_wmutex);
    sem_close(sem_readTry);
    sem_close(sem_wmutex);
    sem_close(sem_rsc);
}
void ReadWholeFile(const char *directory, char filename[], int clientFd)
{
    char readTry[110] = "";
    char rmutex[110] = "";
    char rsc[110] = "";
    char path[100];
    char shm_name[110] = ";";
    char count_sem_name[110];

    sprintf(path, "%s/%s", directory, filename);
    sprintf(count_sem_name, "%s_%s_count", directory, filename);

    sprintf(readTry, "%s_%s_readTry", directory, filename);
    sprintf(rmutex, "%s_%s_rmutex", directory, filename);
    sprintf(rsc, "%s_%s_rsc", directory, filename);
    // set semaphores unique to this file for synchronization
    sem_t *sem_readTry = sem_open(readTry, O_CREAT, 0600, 1);
    sem_t *sem_rmutex = sem_open(rmutex, O_CREAT, 0600, 1);
    sem_t *sem_rsc = sem_open(rsc, O_CREAT, 0600, 1);

    sem_wait(sem_readTry);
    sem_wait(sem_rmutex);
    sprintf(shm_name, "%s_%s_shm_read", directory, filename);
    int fd_read = shm_open(shm_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    int readerCount = GetFileWRCount(fd_read) + 1;
    ChangeFileWRCount(fd_read, count_sem_name, 1);
    if (readerCount == 1)
    {
        sem_wait(sem_rsc);
    }
    sem_post(sem_rmutex);
    sem_post(sem_readTry);

    FILE *fptr;
    char buffer[MAX_CHAR_IN_LINE];
    struct response res;

    if (IsFileCreated(path))
    {
        fptr = fopen(path, "rb");
        int readSize = 0;
        while ((readSize = fread(buffer, 1, sizeof(buffer) - 1, fptr)) > 0)
        {
            memset(res.message, 0, sizeof(res.message));
            memcpy(res.message, buffer, readSize);
            write(clientFd, &res, sizeof(struct response));
        }
        memset(res.message, 0, sizeof(res.message));
        memcpy(res.message, "FILE_READ_SUCCESSFULLY", 23);
        write(clientFd, &res, sizeof(struct response));
        fclose(fptr);
    }
    else
    {
        strcpy(res.message, "INVALID_FILE_NAME");
        write(clientFd, &res, sizeof(struct response));
    }
    sem_wait(sem_rmutex);
    // decrease reader count
    readerCount = ChangeFileWRCount(fd_read, count_sem_name, -1);
    // printf("reader done count: %d\n", readerCount);
    if (readerCount == 0)
    {
        sem_post(sem_rsc);
    }
    sem_post(sem_rmutex);
    sem_close(sem_readTry);
    sem_close(sem_rmutex);
    sem_close(sem_rsc);
}
void ReadLine(const char *directory, char filename[], int line, int clientFd)
{
    char readTry[110] = "";
    char rmutex[110] = "";
    char rsc[110] = "";
    char shm_name[110] = "";
    char count_sem_name[110];
    char path[100];

    sprintf(path, "%s/%s", directory, filename);
    sprintf(count_sem_name, "%s_%s_count", directory, filename);

    sprintf(readTry, "%s_%s_readTry", directory, filename);
    sprintf(rmutex, "%s_%s_rmutex", directory, filename);
    sprintf(rsc, "%s_%s_rsc", directory, filename);

    sem_t *sem_readTry = sem_open(readTry, O_CREAT, 0600, 1);
    sem_t *sem_rmutex = sem_open(rmutex, O_CREAT, 0600, 1);
    sem_t *sem_rsc = sem_open(rsc, O_CREAT, 0600, 1);

    sem_wait(sem_readTry);
    sem_wait(sem_rmutex);
    sprintf(shm_name, "%s_%s_shm_read", directory, filename);
    int fd_read = shm_open(shm_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    int readerCount = GetFileWRCount(fd_read) + 1;
    ChangeFileWRCount(fd_read, count_sem_name, 1);
    // printf("reader count:%d\n", readerCount);

    if (readerCount == 1)
    {
        sem_wait(sem_rsc);
    }
    sem_post(sem_rmutex);
    sem_post(sem_readTry);

    // reading starts from here
    FILE *fptr;
    struct response res;
    char message[MESSAGE_LEN] = "";

    if (IsFileCreated(path))
    {
        if (line <= 0)
        {
            sprintf(message, "Invalid line number");
            memset(res.message, 0, sizeof(res.message));
            memcpy(res.message, message, sizeof(message));
            write(clientFd, &res, sizeof(struct response));
        }
        else
        {
            fptr = fopen(path, "rb");
            int linecount = 1;
            char c = 'c';
            int index = 0;
            // count the lines and find the requested line
            for (; c != EOF; c = getc(fptr))
            {
                if (c == '\n')
                {
                    linecount++;
                }
                if (linecount == line)
                {
                    while ((c = getc(fptr)) != EOF)
                    {
                        if (c != '\n')
                        {
                            message[index] = c;
                            index++;
                        }
                        else
                        {
                            message[index] = '\0';
                            break;
                        }
                    }
                    memcpy(res.message, message, sizeof(message));
                    write(clientFd, &res, sizeof(struct response));
                    break;
                }
            }
            // line number is more than the file has
            if (line > 0 && linecount != line)
            {
                sprintf(message, "Invalid line number");
                memset(res.message, 0, sizeof(res.message));
                memcpy(res.message, message, sizeof(message));
                write(clientFd, &res, sizeof(struct response));
            }

            fclose(fptr);
        }
    }
    // file does not exist
    else
    {
        sprintf(message, "Invalid file");
        memset(res.message, 0, sizeof(res.message));
        memcpy(res.message, message, sizeof(message));
        write(clientFd, &res, sizeof(struct response));
    }
    // reading ended
    sem_wait(sem_rmutex);

    readerCount = ChangeFileWRCount(fd_read, count_sem_name, -1);
    // printf("reader done count: %d\n", readerCount);
    if (readerCount == 0)
    {
        sem_post(sem_rsc);
    }
    sem_post(sem_rmutex);
    sem_close(sem_readTry);
    sem_close(sem_rmutex);
    sem_close(sem_rsc);
}

void HelpDetailed(char argument[], int clientFd)
{
    struct response res;
    char message[MESSAGE_LEN];
    memset(res.message, 0, sizeof(res.message));
    if (strcmp(argument, "list") == 0)
    {
        memcpy(res.message, "sends a request to display the list of files in Servers directory(also displays the list received from the Server)", 115);
    }
    else if (strcmp(argument, "readF") == 0)
    {
        memcpy(res.message, "requests to display the # line of the <file>, if no line number is giventhe whole contents of the file is requested (and displayed on the client side)", 151);
    }
    else if (strcmp(argument, "writeT") == 0)
    {
        memcpy(res.message, "request to write the content of “string” to the #th line the <file>, if the line # is not given writes to the end of file. If the file does not exists in Servers directory creates and edits the file at the same time", 220);
    }
    else if (strcmp(argument, "upload") == 0)
    {
        memcpy(res.message, "uploads the file from the current working directory of client to the Servers directory (beware of the cases no file in clients current working directory and file with the same name on Servers side)", 198);
    }
    else if (strcmp(argument, "download") == 0)
    {
        memcpy(res.message, "request to receive <file> from Servers directory to client side", 64);
    }
    else if (strcmp(argument, "quit") == 0)
    {
        memcpy(res.message, "Send write request to Server side log file and quits", 53);
    }
    else if (strcmp(argument, "killServer") == 0)
    {
        memcpy(res.message, "Sends a kill request to the Server", 35);
    }
    else
    {
        memcpy(res.message, "Invalid command, can't help you.", 33);
    }

    write(clientFd, &res, sizeof(struct response));
}

void DownloadFile(const char *directory, char filename[], int clientFd)
{
    char readTry[110] = "";
    char rmutex[110] = "";
    char rsc[110] = "";
    char shm_name[110] = "";
    char count_sem_name[110];
    char path[100];

    sprintf(path, "%s/%s", directory, filename);
    sprintf(count_sem_name, "%s_%s_count", directory, filename);

    sprintf(readTry, "%s_%s_readTry", directory, filename);
    sprintf(rmutex, "%s_%s_rmutex", directory, filename);
    sprintf(rsc, "%s_%s_rsc", directory, filename);
    // semaphores for synchronization, unique to this file name
    sem_t *sem_readTry = sem_open(readTry, O_CREAT, 0600, 1);
    sem_t *sem_rmutex = sem_open(rmutex, O_CREAT, 0600, 1);
    sem_t *sem_rsc = sem_open(rsc, O_CREAT, 0600, 1);

    sem_wait(sem_readTry);
    sem_wait(sem_rmutex);
    sprintf(shm_name, "%s_%s_shm_read", directory, filename);
    int fd_read = shm_open(shm_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    int readerCount = GetFileWRCount(fd_read) + 1;
    ChangeFileWRCount(fd_read, count_sem_name, 1);
    // printf("reader count:%d\n", readerCount);

    if (readerCount == 1)
    {
        sem_wait(sem_rsc);
    }
    sem_post(sem_rmutex);
    sem_post(sem_readTry);

    FILE *fptr;
    char buffer[MESSAGE_LEN];
    struct response res;
    // download starts
    if (IsFileCreated(path))
    {
        fptr = fopen(path, "rb");
        int readSize = 0;
        while ((readSize = fread(buffer, 1, sizeof(buffer) - 1, fptr)) > 0)
        {
            memset(res.message, 0, sizeof(res.message));
            memcpy(res.message, buffer, readSize);
            res.len = readSize;
            write(clientFd, &res, sizeof(struct response));
        }
        strcpy(res.message, "DOWNLOAD_FINISHED");
        write(clientFd, &res, sizeof(struct response));
        fclose(fptr);
    }
    else
    {
        strcpy(res.message, "FILE_DOESNT_EXIST");
        write(clientFd, &res, sizeof(struct response));
    }
    // download end
    sem_wait(sem_rmutex);
    // decrease reader count, download is counted as reading
    readerCount = ChangeFileWRCount(fd_read, count_sem_name, -1);
    // printf("reader done count: %d\n", readerCount);
    if (readerCount == 0)
    {
        sem_post(sem_rsc);
    }
    sem_post(sem_rmutex);
    sem_close(sem_readTry);
    sem_close(sem_rmutex);
    sem_close(sem_rsc);
}

void UploadFile(char filename[], int clientWRFd, int clientFd)
{
    struct response res;

    read(clientFd, &res, sizeof(struct response));
    if (strcmp(res.message, "FILE_EXIST") == 0)
    {
        printf("File with this name exists in server directory.\n");
        return;
    }

    char readTry[110] = "";
    char rmutex[110] = "";
    char rsc[110] = "";
    char shm_name[110] = "";
    char count_sem_name[110];
    char path[100];

    sprintf(path, "%s", filename);
    sprintf(count_sem_name, "%s_count", filename);

    sprintf(readTry, "%s_readTry", filename);
    sprintf(rmutex, "%s_rmutex", filename);
    sprintf(rsc, "%s_rsc", filename);
    // semaphores for synchronization, unique to this file name
    sem_t *sem_readTry = sem_open(readTry, O_CREAT, 0600, 1);
    sem_t *sem_rmutex = sem_open(rmutex, O_CREAT, 0600, 1);
    sem_t *sem_rsc = sem_open(rsc, O_CREAT, 0600, 1);

    sem_wait(sem_readTry);
    sem_wait(sem_rmutex);
    sprintf(shm_name, "%s_shm_read", filename);
    int fd_read = shm_open(shm_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    int readerCount = GetFileWRCount(fd_read) + 1;
    ChangeFileWRCount(fd_read, count_sem_name, 1);

    if (readerCount == 1)
    {
        sem_wait(sem_rsc);
    }
    sem_post(sem_rmutex);
    sem_post(sem_readTry);

    FILE *fptr;
    char buffer[MESSAGE_LEN];
    // upload starts
    if (IsFileCreated(path))
    {
        fptr = fopen(path, "rb");
        int readSize = 0;
        while ((readSize = fread(buffer, 1, sizeof(buffer) - 1, fptr)) > 0)
        {
            memset(res.message, 0, sizeof(res.message));
            memcpy(res.message, buffer, readSize);
            res.len = readSize;
            write(clientWRFd, &res, sizeof(struct response));
        }
        strcpy(res.message, "UPLOAD_FINISHED");
        write(clientWRFd, &res, sizeof(struct response));
        fclose(fptr); //
        printf("File uploaded\n");
    }
    else
    {
        printf("File does not exist in client directory.\n");
        strcpy(res.message, "FILE_DOESNT_EXIST");
        write(clientWRFd, &res, sizeof(struct response));
    }
    // upload end
    sem_wait(sem_rmutex);
    // decrease reader count, upload is counted as reading
    readerCount = ChangeFileWRCount(fd_read, count_sem_name, -1);
    if (readerCount == 0)
    {
        sem_post(sem_rsc);
    }
    sem_post(sem_rmutex);
    sem_close(sem_readTry);
    sem_close(sem_rmutex);
    sem_close(sem_rsc);
}

void ListFiles(const char *directory, int clientFd)
{
    char buffer[512];
    DIR *d;
    struct dirent *dir;
    char dirPath[256] = "";
    sprintf(dirPath, "./%s", directory);
    d = opendir(directory);
    struct response res;
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            // only send files, exclude directories etc.
            if (dir->d_type == 8)
            {
                // send the files one by one, client will read one by one as well
                strcpy(res.message, dir->d_name);
                write(clientFd, &res, sizeof(struct response));
            }
        }
        strcpy(res.message, "success: all files are printed.");
        write(clientFd, &res, sizeof(struct response));
    }
    else
    {
        strcpy(res.message, "failure: files couldn't be printed.");
        write(clientFd, &res, sizeof(struct response));
    }
    closedir(d);
}
// if the pid is in array returns the index, otherwise returns -1
int FindInClientArray(ClientInfo infos[], int len, int pid)
{
    for (int i = 0; i < len; ++i)
    {
        if (infos[i].pid == pid)
        {
            return i;
        }
    }
    return -1;
}
void CreateLogFile(const char *directory, int serverPid)
{
    FILE *logfptr;
    char logfileName[50];
    sprintf(logfileName, "%s/log_%d", directory, serverPid);
    logfptr = fopen(logfileName, "w");
    fclose(logfptr);
}
void WriteToLogFile(const char *directory, int serverPid, char message[])
{
    FILE *logfptr;
    char logfileName[50];
    sprintf(logfileName, "%s/log_%d", directory, serverPid);
    logfptr = fopen(logfileName, "a");
    fprintf(logfptr, "%s\n", message);
    fclose(logfptr);
}
#endif