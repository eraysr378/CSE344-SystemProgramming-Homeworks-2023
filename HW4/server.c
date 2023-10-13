#define _POSIX_C_SOURCE 200112L
// base64 /dev/urandom | head -c 10000000 > file.txt
#include "functions.h"
#include "mytypes.h"
#include "myconst.h"
#include "taskqueue.h"
#include "pidqueue.h"
int POOL_SIZE;
int serverFd, dummyFd;
pthread_t *threads;
int numClients = 0;
int clientNameCount = 0;
int MAX_CLIENT;
int killSelf = 0;
int killThreads = 0;
ClientInfo *connectedClients;
const char *directory;
pthread_mutex_t mutexQueue;
pthread_cond_t condQueue;
void executeTask(Task *task)
{
    char message[600];
    int index = FindInClientArray(connectedClients, MAX_CLIENT, task->pid);

    sprintf(message, "client%d requested:%s", connectedClients[index].num, task->request);
    WriteToLogFile(directory, getpid(), message);

    struct response res;
    char clientFifo[CLIENT_FIFO_NAME_LEN];
    snprintf(clientFifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, (long)task->pid);
    int clientFd = open(clientFifo, O_WRONLY);
    char command[512];
    strcpy(command, task->request);

    char *ptr = strtok(command, " ");
    int argumentNum = 0;
    char arguments[3][50];
    char stringToWrite[MESSAGE_LEN] = "";
    // parse the command
    for (int i = 0; i < 4; i++)
    {

        if (ptr != NULL)
        {
            argumentNum++;
            // these are needed because program should be able to get multiple words (string) in writeT
            if (i < 3)
                strcpy(arguments[i], ptr);
            else
            {
                ptr = strtok(NULL, "");
                if (ptr != NULL)
                {
                    strcpy(stringToWrite, ptr);
                }
                else
                {
                    argumentNum--;
                }
            }
            if (i < 2)
                ptr = strtok(NULL, " ");
        }
    }
    // this is needed to discriminate which version of writeT is given
    if (argumentNum > 2 && strcmp(arguments[0], "writeT") == 0)
    {
        int line = atoi(arguments[2]);
        if (line == 0)
        {
            argumentNum = 3;
        }
    }
    // printf("%s,%s,%s,%s,%d\n",arguments[0],arguments[1],arguments[2],stringToWrite,argumentNum);

    memset(res.message, 0, sizeof(res.message));
    if (clientFd == -1)
    {
        fprintf(stderr, "client fifo couldn't open\n");
        return;
    }
    if (argumentNum == 1 && strcmp(task->request, "help") == 0)
    {
        memcpy(res.message, "help\nlist\nreadF <file> <line #>\nwriteT <file> <line #> <string>\nupload <file>\nupload <file> -ow\nupload <file> <newfilename>\ndownload <file>\ndownload <file> -ow\ndownload <file> <newfilename>\nquit\nkillServer", 208);
        write(clientFd, &res, sizeof(struct response));
    }
    else if (argumentNum == 2 && strcmp(arguments[0], "help") == 0)
    {
        HelpDetailed(arguments[1], clientFd);
    }

    else if (argumentNum == 1 && strcmp(task->request, "list") == 0)
    {
        ListFiles(directory, clientFd);
    }
    else if (argumentNum == 1 && strcmp(task->request, "quit") == 0)
    {

        int index = FindInClientArray(connectedClients, MAX_CLIENT, task->pid);
        char message[100];
        // if waiting client wants to quit (kill signal) remove from queue
        if (index == -1)
        {
            strcpy(res.message, "leaving.");
            write(clientFd, &res, sizeof(struct response));
            RemoveFromPidQueue(task->pid);
        }
        else
        {
            strcpy(res.message, "leaving.");
            write(clientFd, &res, sizeof(struct response));
            numClients--;
            sprintf(message, "client%d disconnected\n", connectedClients[index].num);
            printf("%s\n", message);
            WriteToLogFile(directory, getpid(), message);
            // remove the client from connected clients array
            connectedClients[index].num = -1;
            connectedClients[index].pid = -1;
            // if there is waiting client, let them in
            if (FrontOfPidQueue() != -1)
            {
                int frontPid = DequeuePid();
                char newClientFifo[CLIENT_FIFO_NAME_LEN];
                snprintf(newClientFifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, (long)frontPid);
                int newClientFd = open(newClientFifo, O_WRONLY);
                struct response resp;
                strcpy(resp.message, "CONNECTION_SUCCESSFULL");
                write(newClientFd, &resp, sizeof(struct response));
                numClients++;
                clientNameCount++;

                char message[100];
                sprintf(message, "Client PID:%d connected as \"client%d\"", frontPid, clientNameCount);
                printf("%s\n", message);
                WriteToLogFile(directory, getpid(), message);
                int index = FindInClientArray(connectedClients, MAX_CLIENT, -1);
                connectedClients[index].num = clientNameCount;
                connectedClients[index].pid = frontPid;
                close(newClientFd);
            }
        }
        close(clientFd);
        unlink(clientFifo);
    }

    // write to the end of the file
    else if (argumentNum == 3 && strcmp(arguments[0], "writeT") == 0)
    {
        char message[566];
        sprintf(message, "\n%s %s", arguments[2], stringToWrite);
        AppendToFile(directory, arguments[1], message);
        memcpy(res.message, "Successfully written.", 22);
        write(clientFd, &res, sizeof(struct response));
    }
    // write to a line of the file
    else if (argumentNum == 4 && strcmp(arguments[0], "writeT") == 0)
    {
        char message[566];
        sprintf(message, "%s", stringToWrite);
        int lineNum = atoi(arguments[2]);
        WriteToLine(directory, arguments[1], lineNum, message, task->pid);
        memcpy(res.message, "Successfully written.", 22);
        write(clientFd, &res, sizeof(struct response));
    }
    // read a whole file
    else if (argumentNum == 2 && strcmp(arguments[0], "readF") == 0)
    {
        ReadWholeFile(directory, arguments[1], clientFd);
    }
    // read a line
    else if (argumentNum == 3 && strcmp(arguments[0], "readF") == 0)
    {
        int line = atoi(arguments[2]);
        ReadLine(directory, arguments[1], line, clientFd);
    }
    // download
    else if ((argumentNum == 2 || argumentNum == 3) && strcmp(arguments[0], "download") == 0)
    {
        DownloadFile(directory, arguments[1], clientFd);
    }
    // upload
    else if ((argumentNum == 2 || argumentNum == 3) && strcmp(arguments[0], "upload") == 0)
    {
        char fileName[50];
        char filePath[100];

        if (argumentNum == 2)
            strcpy(fileName, arguments[1]);
        else if (argumentNum == 3 && strcmp(arguments[2], "-ow") == 0)
            strcpy(fileName, arguments[1]);
        else
            strcpy(fileName, arguments[2]);

        fileName[strcspn(fileName, "\n")] = 0; // remove \n at the end
        struct response resp;

        // check if the file is created previously.
        if (!(argumentNum == 3 && strcmp(arguments[2], "-ow") == 0) && SeekFileInGivenDir(fileName, directory) == 1)
        {
            strcpy(res.message, "FILE_EXIST");
            write(clientFd, &res, sizeof(struct response));
            return;
        }
        else
        {
            strcpy(res.message, "FILE_NOT_EXIST");
            write(clientFd, &res, sizeof(struct response));
        }
        sprintf(filePath, "%s/%s", directory, fileName);

        FILE *fptr;
        fptr = fopen(filePath, "wb");
        close(clientFd);
        clientFd = open(clientFifo, O_RDONLY);

        while (1)
        {
            read(clientFd, &resp, sizeof(struct response));
            if (strcmp(resp.message, "UPLOAD_FINISHED") != 0 && strcmp(resp.message, "FILE_DOESNT_EXIST") != 0)
            {
                fwrite(resp.message, 1, resp.len, fptr);
            }
            else if (strcmp(resp.message, "UPLOAD_FINISHED") == 0)
            {
                break;
            }
            else
            {
                remove(filePath);
                break;
            }
        }
        close(clientFd);
        fclose(fptr);
    }
    else
    {
        memcpy(res.message, "Invalid command, to see list of the commands use help.", 55);
        write(clientFd, &res, sizeof(struct response));
    }

    close(clientFd);
}
void *startThread(void *args)
{
    while (1)
    {
        pthread_mutex_lock(&mutexQueue);
        if (killThreads == 1)
        {
            pthread_mutex_unlock(&mutexQueue);
            pthread_cond_broadcast(&condQueue);
            return NULL;
        }
        while (FrontOfTaskQueue().pid == -1)
        {
            pthread_cond_wait(&condQueue, &mutexQueue);
            if (killThreads == 1)
            {
                pthread_mutex_unlock(&mutexQueue);
                pthread_cond_broadcast(&condQueue);
                return NULL;
            }
        }
        Task task = DequeueTask();
        pthread_mutex_unlock(&mutexQueue);
        executeTask(&task);
    }
}

void ServerSignalHandler(int signo)
{
    switch (signo)
    {
    case SIGINT:
        // kill connected clients
        for (int i = 0; i < MAX_CLIENT; i++)
        {
            if (connectedClients[i].pid != -1)
            {
                char clientFifo[CLIENT_FIFO_NAME_LEN];
                snprintf(clientFifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, (long)connectedClients[i].pid);
                kill(connectedClients[i].pid, SIGKILL);
                unlink(clientFifo);
            }
        }
        // kill clients that are waiting to connect
        struct response resp;
        while (FrontOfPidQueue() != -1)
        {
            int frontPid = DequeuePid();
            char clientFifo[CLIENT_FIFO_NAME_LEN];
            snprintf(clientFifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, (long)frontPid);
            kill(frontPid, SIGKILL);
            unlink(clientFifo);
        }

        // notify the threads that they should finish executing
        killThreads = 1;
        pthread_cond_broadcast(&condQueue);
        // join the threads to clear memory
        for (int i = 0; i < POOL_SIZE; i++)
        {
            if (pthread_join(threads[i], NULL) != 0)
            {
                printf("waiting\n");
                perror("Failed to join the thread");
            }
        }
        close(serverFd);
        close(dummyFd);
        pthread_mutex_destroy(&mutexQueue);
        pthread_cond_destroy(&condQueue);
        printf("Server has been killed.\n");

        exit(0);

        break;

    default:
        break;
    }
}

int main(int argc, char const *argv[])
{
    if (argc != 4)
    {
        printf("Usage biboServer <dirname> <max. #ofClients> <poolsize");
        exit(0);
    }
    directory = argv[1];
    MAX_CLIENT = atoi(argv[2]);
    POOL_SIZE = atoi(argv[3]);
    // initialize connected clients
    ClientInfo clients[MAX_CLIENT];
    for (int i = 0; i < MAX_CLIENT; i++)
    {
        clients[i].pid = -1;
        clients[i].num = -1;
    }
    // make it accessible by thread function
    connectedClients = clients;

    if (mkdir(argv[1], S_IRWXU | S_IRWXG | S_IRWXO) == -1)
    {
        if (errno != 17)
        {
            printf("Error:%s", strerror(errno));
            exit(-1);
        }
        else
        {
            errno = 0;
        }
    }
    printf("Server Started PID:%d ...\n", getpid());
    printf("Waiting for clients...\n");
    CreateLogFile(directory, getpid());

    char server_pid[20];
    sprintf(server_pid, "%d", getpid());
    // signal handling
    struct sigaction server_sa;
    memset(&server_sa, 0, sizeof(server_sa));
    server_sa.sa_handler = &ServerSignalHandler;
    sigaction(SIGINT, &server_sa, NULL);

    pthread_t th[POOL_SIZE];
    pthread_mutex_init(&mutexQueue, NULL);
    pthread_cond_init(&condQueue, NULL);

    int i;
    for (i = 0; i < POOL_SIZE; i++)
    {
        if (pthread_create(&th[i], NULL, &startThread, NULL) != 0)
        {
            perror("Failed to create the thread");
        }
    }
    // used in signal handler
    threads = th;

    int clientFd;
    char clientFifo[CLIENT_FIFO_NAME_LEN];
    char serverFifo[SERVER_FIFO_NAME_LEN];
    struct request req;
    struct response resp;
    umask(0);
    snprintf(serverFifo, SERVER_FIFO_NAME_LEN, SERVER_FIFO_TEMPLATE, (long)getpid());

    if (mkfifo(serverFifo, S_IRUSR | S_IWUSR | S_IWGRP) == -1 && errno != EEXIST)
    {
        fprintf(stderr, "mkfifo: %s\n", serverFifo);
    }

    serverFd = open(serverFifo, O_RDONLY);
    if (serverFd == -1)
    {
        printf("server fd error");
        exit(-1);
    }
    dummyFd = open(serverFifo, O_WRONLY);
    if (dummyFd == -1)
    {
        exit(-2);
    }
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
    {
        exit(-3);
    }

    while (1)
    {
        fflush(stdout);
        if (read(serverFd, &req, sizeof(struct request)) != sizeof(struct request))
        {
            printf("err:%s\n", strerror(errno));
            exit(-1);
        }

        if (strcmp(req.message, "CLIENT_CONNECTION_REQUEST") == 0)
        {
            snprintf(clientFifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, (long)req.pid);
            clientFd = open(clientFifo, O_WRONLY);
            if (numClients < MAX_CLIENT)
            {

                strcpy(resp.message, "CONNECTION_SUCCESSFULL");
                write(clientFd, &resp, sizeof(struct response));
                numClients++;
                clientNameCount++;
                int index = FindInClientArray(connectedClients, MAX_CLIENT, -1);
                connectedClients[index].num = clientNameCount;
                connectedClients[index].pid = req.pid;
                char message[100];
                sprintf(message, "Client PID:%d connected as \"client%d\"", req.pid, clientNameCount);
                printf("%s\n", message);
                WriteToLogFile(directory, getpid(), message);
            }
            else
            {
                EnqueuePid(req.pid);
            }
            close(clientFd);
        }
        else if (strcmp(req.message, "CLIENT_TRY_CONNECTION_REQUEST") == 0)
        {
            printf("client try connection request received.\n");
            if (numClients < MAX_CLIENT)
            {
                strcpy(resp.message, "CONNECTION_SUCCESSFULL");
                numClients++;
                clientNameCount++;
                int index = FindInClientArray(connectedClients, MAX_CLIENT, -1);
                connectedClients[index].num = clientNameCount;
                connectedClients[index].pid = req.pid;
                char message[100];
                sprintf(message, "Client PID:%d connected as \"client%d\"", req.pid, clientNameCount);
                printf("%s\n", message);
                WriteToLogFile(directory, getpid(), message);
            }
            else
            {
                strcpy(resp.message, "SERVER_FULL");
            }
            snprintf(clientFifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, (long)req.pid);
            clientFd = open(clientFifo, O_WRONLY);
            write(clientFd, &resp, sizeof(struct response));
            close(clientFd);
        }
        else if (strcmp(req.message, "killServer") == 0)
        {
            char message[600];
            int index = FindInClientArray(connectedClients, MAX_CLIENT, req.pid);
            sprintf(message, "client%d requested:killServer", connectedClients[index].num);
            WriteToLogFile(directory, getpid(), message);
            fprintf(stdout, "Kill signal from client%d\n", connectedClients[index].num);
            // kill connected clients
            for (int i = 0; i < MAX_CLIENT; i++)
            {
                if (connectedClients[i].pid != -1)
                {
                    char clientFifo[CLIENT_FIFO_NAME_LEN];
                    snprintf(clientFifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, (long)connectedClients[i].pid);
                    kill(connectedClients[i].pid, SIGKILL);
                    unlink(clientFifo);
                }
            }
            // kill clients that are waiting to connect
            struct response resp;
            while (FrontOfPidQueue() != -1)
            {
                int frontPid = DequeuePid();
                char clientFifo[CLIENT_FIFO_NAME_LEN];
                snprintf(clientFifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, (long)frontPid);
                kill(frontPid, SIGKILL);
                unlink(clientFifo);
            }
            break;
        }
        else
        {
            Task newTask;
            newTask.pid = req.pid;
            memcpy(newTask.request, req.message, sizeof(req.message));
            pthread_mutex_lock(&mutexQueue);
            EnqueueTask(newTask);
            pthread_mutex_unlock(&mutexQueue);
            pthread_cond_signal(&condQueue);
        }
    }
    // notify the threads that they should finish executing
    killThreads = 1;
    pthread_cond_broadcast(&condQueue);
    // join the threads to clear memory
    for (int i = 0; i < POOL_SIZE; i++)
    {
        if (pthread_join(th[i], NULL) != 0)
        {
            perror("Failed to join the thread");
        }
    }
    close(serverFd);
    close(dummyFd);
    pthread_mutex_destroy(&mutexQueue);
    pthread_cond_destroy(&condQueue);
    return 0;
}
