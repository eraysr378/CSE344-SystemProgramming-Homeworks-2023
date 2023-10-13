#include "functions.h"

static char clientFifo[CLIENT_FIFO_NAME_LEN];
static char serverFifo[SERVER_FIFO_NAME_LEN];
bool reading = false;
bool writing = false;
bool waiting = true;
bool killSelf = false;
int serverFd, clientFd;

void ClientSignalHandler(int signo)
{
    switch (signo)
    {
    case SIGINT:
        if (reading || writing)
        {
            killSelf = true;
        }
        else
        {
            printf("\nKill signal received, terminating...\n");
            struct request req;
            req.pid = getpid();
            strcpy(req.message, "quit");
            // tell the server that you left the server
            if (write(serverFd, &req, sizeof(struct request)) != sizeof(struct request))
            {
                printf("write error");
                exit(-3);
            }
            struct response res;
            // get the message that is sent from server to not block it
            read(clientFd, &res, sizeof(struct response));
            exit(0);
        }

        break;
    default:
        break;
    }
}

int main(int argc, char const *argv[])
{
    if (argc != 3)
    {
        printf("Usage: biboClient <Connect/tryConnect> ServerPID");
        return 0;
    }

    struct request req;
    struct response resp;
    char reqMessage[512];
    int serverPid = atoi(argv[2]);

    // signal handling
    struct sigaction client_sa;
    memset(&client_sa, 0, sizeof(client_sa));
    client_sa.sa_handler = &ClientSignalHandler;
    sigaction(SIGINT, &client_sa, NULL);

    umask(0);

    snprintf(clientFifo, CLIENT_FIFO_NAME_LEN, CLIENT_FIFO_TEMPLATE, (long)getpid());
    snprintf(serverFifo, SERVER_FIFO_NAME_LEN, SERVER_FIFO_TEMPLATE, (long)serverPid);

    if (mkfifo(clientFifo, S_IRUSR | S_IWUSR | S_IWGRP) == -1)
    {
        exit(-1);
    }

    req.pid = getpid();
    serverFd = open(serverFifo, O_WRONLY);

    if (serverFd == -1)
    {
        printf("server fd is -1\n");

        exit(-2);
    }
    // set the connection message
    if (strcmp(argv[1], "Connect") == 0)
    {
        sprintf(req.message, "CLIENT_CONNECTION_REQUEST");
    }
    else if (strcmp(argv[1], "tryConnect") == 0)
    {
        sprintf(req.message, "CLIENT_TRY_CONNECTION_REQUEST");
    }
    else
    {
        printf("Invalid connection option, use connect or tryConnect\n");
        close(clientFd);
        close(serverFd);
        unlink(clientFifo);
        return 0;
    }
    if (write(serverFd, &req, sizeof(struct request)) != sizeof(struct request))
    {
        printf("write error");
        exit(-3);
    }

    clientFd = open(clientFifo, O_RDONLY);
    // open client fd in write mode as well to not to block it
    int clientWRFd = open(clientFifo, O_WRONLY);
    if (strcmp(argv[1], "Connect") == 0)
    {
        do
        {
            printf("waiting to connect\n");
            read(clientFd, &resp, sizeof(struct response));
            if (strcmp(resp.message, "killServer") == 0)
            {
                printf("Server has been killed.\n");
                close(clientFd);
                close(clientWRFd);
                close(serverFd);
                unlink(clientFifo);
                return 0;
            }
        } while (strcmp(resp.message, "CONNECTION_SUCCESSFULL") != 0);
    }
    else if (strcmp(argv[1], "tryConnect") == 0)
    {

        read(clientFd, &resp, sizeof(struct response));
        if (strcmp(resp.message, "CONNECTION_SUCCESSFULL") != 0)
        {
            printf("server full.\n");
            close(clientFd);
            close(clientWRFd);
            close(serverFd);
            unlink(clientFifo);
            return 0;
        }
    }
    else
    {
        printf("Invalid connection option, use connect or tryConnect\n");
        close(clientFd);
        close(clientWRFd);
        close(serverFd);
        unlink(clientFifo);
        return 0;
    }
    if (clientFd == -1)
    {
        printf("cllient fd:");
        exit(-4);
    }
    waiting = false;
    while (1)
    {
        if (killSelf)
        {
            printf("\nKill signal received, terminating...\n");
            struct request req;
            req.pid = getpid();
            strcpy(req.message, "quit");

            int serverFd = open(serverFifo, O_WRONLY);
            // tell the server that you left the server
            if (write(serverFd, &req, sizeof(struct request)) != sizeof(struct request))
            {
                printf("write error");
                exit(-3);
            }
            exit(0);
        }
        char command[512];
        printf("\n\nEnter command:");
        fgets(command, 512, stdin);
        command[strcspn(command, "\n")] = 0;
        char copy_command[512];
        strcpy(copy_command, command);

        char *ptr = strtok(copy_command, " ");
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
        strcpy(req.message, command);
        req.pid = getpid();
        if ((argumentNum == 2 || argumentNum == 3) && strcmp(arguments[0], "download") == 0)
        {
            char fileName[50];
            if (argumentNum == 2)
                strcpy(fileName, arguments[1]);
            else if (argumentNum == 3 && strcmp(arguments[2], "-ow") == 0)
                strcpy(fileName, arguments[1]);
            else
                strcpy(fileName, arguments[2]);

            fileName[strcspn(fileName, "\n")] = 0; // remove \n at the end
            // if file exists, and overwrite option is not used
            if (!(argumentNum == 3 && strcmp(arguments[2], "-ow") == 0) && SeekFileInCurrentDir(fileName) == 1)
            {
                printf("\nFile exists in your directory.Try changing the filename or use overwrite option.\n");
                printf("new file name usage: download <filename> <newfilename>\n");
                printf("overwrite usage: download <fileName> -ow\n");
                continue;
            }
        }
        if (write(serverFd, &req, sizeof(struct request)) != sizeof(struct request))
        {
            printf("write error");
            exit(-3);
        }

        if (strcmp(command, "quit") == 0)
        {
            read(clientFd, &resp, sizeof(struct response));
            printf("Leaving...\n");
            break;
        }
        else if (argumentNum == 2 && strcmp(arguments[0], "readF") == 0)
        {
            reading = true;
            while (1)
            {
                read(clientFd, &resp, sizeof(struct response));
                if (strcmp(resp.message, "FILE_READ_SUCCESSFULLY") != 0 && strcmp(resp.message, "INVALID_FILE_NAME") != 0)
                {
                    printf("%s", resp.message);
                }
                else if (strcmp(resp.message, "INVALID_FILE_NAME") == 0)
                {
                    printf("File does not exist in server directory.\n");
                    break;
                }
                else
                {
                    break;
                }
            }
            reading = false;
        }
        else if ((argumentNum == 2 || argumentNum == 3) && strcmp(arguments[0], "download") == 0)
        {
            reading = true;
            char fileName[50];

            if (argumentNum == 2)
                strcpy(fileName, arguments[1]);
            else if (argumentNum == 3 && strcmp(arguments[2], "-ow") == 0)
                strcpy(fileName, arguments[1]);
            else
                strcpy(fileName, arguments[2]);

            fileName[strcspn(fileName, "\n")] = 0; // remove \n at the end

            FILE *fptr;
            fptr = fopen(fileName, "wb");
            while (1)
            {
                read(clientFd, &resp, sizeof(struct response));
                if (strcmp(resp.message, "DOWNLOAD_FINISHED") != 0 && strcmp(resp.message, "FILE_DOESNT_EXIST") != 0)
                {
                    fwrite(resp.message, 1, resp.len, fptr);
                }
                else if (strcmp(resp.message, "DOWNLOAD_FINISHED") == 0)
                {
                    printf("Download finished successfully.\n");
                    break;
                }
                else
                {
                    remove(fileName);
                    printf("File does not exist in server directory.\n");
                    break;
                }
            }
            fclose(fptr);
            reading = false;
        }
        else if ((argumentNum == 2 || argumentNum == 3) && strcmp(arguments[0], "upload") == 0)
        {
            writing = true;
            UploadFile(arguments[1], clientWRFd, clientFd);
            writing = false;
        }
        else if (strcmp(command, "list") == 0)
        {
            reading = true;
            do
            {
                read(clientFd, &resp, sizeof(struct response));
                if (strcmp(resp.message, "success: all files are printed.") != 0)
                    printf("%s\n", resp.message);

            } while (strcmp(resp.message, "success: all files are printed.") != 0 && strcmp(resp.message, "failure: files couldn't be printed.") != 0);
            reading = false;
        }
        else
        {
            read(clientFd, &resp, sizeof(struct response));
            printf("%s\n", resp.message);
        }
    }
    close(clientFd);
    close(clientWRFd);
    close(serverFd);
    return 0;
}