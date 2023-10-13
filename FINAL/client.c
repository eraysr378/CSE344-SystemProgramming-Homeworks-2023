#include "clfunctions.h"
#include <pthread.h>
int main(int argc, char const *argv[])
{
    if (argc != 3 && argc != 4)
    {
        printf("Usage: BibakBOXClient [dirName] [portnumber] [optional_server_address]\n");
        return -1;
    }
    char cl_dir[256];
    strcpy(cl_dir, argv[1]);
    DIR *dir;
    if (!(dir = opendir(cl_dir)))
    {
        printf("Invalid directory\n");
        return -1;
    }
    else
        closedir(dir);

    // pthread_t fileCheckThread;
    // if (pthread_create(&fileCheckThread, NULL, &startThread, NULL) != 0)
    // {
    //     perror("Failed to create the thread");
    // }
    char sv_address[25];
    int sockfd, connfd;
    struct sockaddr_in servaddr, cli;
    char buffer[MAX];
    PORT = atoi(argv[2]);

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
    if (argc == 3)
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    else
        servaddr.sin_addr.s_addr = inet_addr(argv[3]);
    servaddr.sin_port = htons(PORT);

    printf("Request sent to the server...\n");

    // connect the client socket to server socket
    if (connect(sockfd, (SA *)&servaddr, sizeof(servaddr)) != 0)
    {
        printf("%d:connection with the server failed...\n", sockfd);
        perror("err");
        exit(0);
    }

    // wait for server response, server will inform the client if there is empty place or not
    if (strcmp("server is full", receiveMsg(sockfd)) == 0)
    {
        printf("server is full leaving...\n");
        close(sockfd);
        return 0;
    }
    printf("Connected to the server.\n");

    // function for chat
    Synchronize(sockfd, cl_dir);

    // close the socket
    close(sockfd);
}