
#ifndef CLIENTQUEUE_H
#define CLIENTQUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <memory.h>
#include "mytypes.h"
#include <unistd.h>

ClientNode *frontClient = NULL;
ClientNode *rearClient = NULL;
int DequeueClient()
{
    if (frontClient == NULL)
    {

        return -1;
    }
    else
    {
        ClientNode *temp;
        int rval = frontClient->sockFd;
        temp = frontClient;
        if (frontClient->next == NULL)
        {
            frontClient = NULL;
            rearClient = NULL;
        }
        else
        {
            frontClient = frontClient->next;
        }
        free(temp);
        return rval;
    }
}
void DisplayClientQueue()
{
    ClientNode *temp = frontClient;
    printf("\nqueue:");
    while (temp != NULL)
    {
        printf("%d,", temp->sockFd);
        temp = temp->next;
    }
    printf("\n");
}
void RemoveFromClientQueue(int sockFd)
{
    if (frontClient == NULL)
    {
        return;
    }
    else if (frontClient->sockFd == sockFd)
    {
        DequeueClient();
    }
    else
    {
        ClientNode *temp;
        temp = frontClient;
        while (temp->next != NULL && temp->next->sockFd != sockFd)
        {
            temp = temp->next;
        }
        if (temp->next == NULL)
        {
            return;
        }
        else
        {
            ClientNode *tempv2;
            tempv2 = temp->next->next;
            free(temp->next);
            temp->next = tempv2;
            while (temp->next != NULL)
            {
                temp = temp->next;
            }
            rearClient = temp;
        }
    }
}
int FrontOfClientQueue()
{
    if (frontClient == NULL)
        return -1;
    else
        return frontClient->sockFd;
}
void EnqueueClient(int sockFd, char claddr[])
{

    ClientNode *nptr = (ClientNode *)malloc(sizeof(ClientNode));
    nptr->sockFd = sockFd;
    strcpy(nptr->addr,claddr);
    nptr->next = NULL;
    if (rearClient == NULL)
    {
        frontClient = nptr;
        rearClient = nptr;
    }
    else
    {
        rearClient->next = nptr;
        rearClient = rearClient->next;
    }
}
bool SearchInClientQueue(int sockFd)
{
    if (frontClient == NULL)
    {
        return false;
    }
    else
    {
        ClientNode *temp;
        temp = frontClient;
        while (temp != NULL)
        {
            if (temp->sockFd == sockFd)
            {
                return true;
            }
            else
            {
                temp = temp->next;
            }
        }
        return false;
    }
}

void CleanClientQueue()
{
    while (FrontOfClientQueue() != -1)
    {
        DequeueClient();
    }
}



#endif