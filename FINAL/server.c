#define _POSIX_C_SOURCE 200112L
#include "svfunctions.h"

char* fnc(){
    char *a = malloc(sizeof(char)*1024);
    strcpy(a,"aaa");
    return a;
}
int main(int argc, char const *argv[])
{
    if (argc != 4)
    {
        printf("Usage: BibakBOXServer [directory] [threadPoolSize] [portnumber]\n");
        return -1;
    }
    POOL_SIZE = atoi(argv[2]);
    PORT = atoi(argv[3]);
    strcpy(SV_DIR, argv[1]);

    if (POOL_SIZE <= 0)
    {
        printf("Consumer pool size should be greater than 0\n");
        return -1;
    }
    pthread_mutex_init(&Mutex_connFd, NULL);
    pthread_mutex_init(&Mutex_sync_or_connect, NULL);
    pthread_mutex_init(&Mutex_fileUpdated, NULL);
    pthread_mutex_init(&Mutex_sync_threads, NULL);
    pthread_mutex_init(&Mutex_fileLastSync, NULL);
    pthread_cond_init(&Cond_fdReady, NULL);
    pthread_cond_init(&Cond_fdWait, NULL);
    pthread_cond_init(&Cond_sync_threads, NULL);
    pthread_cond_init(&Cond_connect_client, NULL);
    pthread_cond_init(&Cond_sync_or_connect, NULL);

    pthread_t threads[POOL_SIZE];
    for (int i = 0; i < POOL_SIZE; i++)
    {
        if (pthread_create(&threads[i], NULL, &startThread, NULL) != 0)
        {
            perror("Failed to create the thread");
        }
    }

    int sockfd, len;
    int connfd;
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");

    memset(&servaddr, 0, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (SA *)&servaddr, sizeof(servaddr))) != 0)
    {
        printf("socket bind failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully binded..\n");

    // Now server is ready to listen and verification
    if ((listen(sockfd, 5)) != 0)
    {
        printf("Listen failed...\n");
        exit(0);
    }
    else
        printf("Server listening..\n");
    len = sizeof(cli);

    while (1)
    {
        // printf("waiting to accept...\n");
        //  Accept the data packet from client and verification
        connfd = accept(sockfd, (SA *)&cli, &len);
        if (connfd < 0)
        {
            printf("server accept failed...\n");
            perror("err:");
            exit(0);
        }
        else
        {
            if (POOL_SIZE <= ConnectedClientNum)
            {
                sendMsg(connfd, "server is full");
                close(connfd);
            }
            else
            {
                char claddr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &cli.sin_addr, claddr, INET_ADDRSTRLEN);
                pthread_mutex_lock(&Mutex_connFd);
                EnqueueClient(connfd, claddr);
                ConnectionRequests++;
                ConnectedClientNum++;
                printf("Client connected:%s.\n", claddr);
                pthread_mutex_unlock(&Mutex_connFd);
                pthread_cond_signal(&Cond_fdReady);
            }
        }
    }

    // clear the memory
    pthread_mutex_destroy(&Mutex_connFd);
    pthread_mutex_destroy(&Mutex_sync_or_connect);
    pthread_mutex_destroy(&Mutex_fileUpdated);
    pthread_mutex_destroy(&Mutex_sync_threads);
    pthread_mutex_destroy(&Mutex_fileLastSync);

    pthread_cond_destroy(&Cond_fdReady);
    pthread_cond_destroy(&Cond_fdWait);
    pthread_cond_destroy(&Cond_sync_threads);
    pthread_cond_destroy(&Cond_connect_client);
    pthread_cond_destroy(&Cond_sync_or_connect);
    CleanClientQueue();
    close(sockfd);
}
