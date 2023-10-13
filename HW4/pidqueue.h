
#ifndef PIDQUEUE_H
#define PIDQUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <memory.h>
#include "mytypes.h"
#include <unistd.h>

struct pidNode *frontPid = NULL;
struct pidNode *rearPid = NULL;
int DequeuePid()
{
    if (frontPid == NULL)
    {

        return -1;
    }
    else
    {
        struct pidNode *temp;
        int rval = frontPid->pid;
        temp = frontPid;
        if (frontPid->next == NULL)
        {
            frontPid = NULL;
            rearPid = NULL;
        }
        else
        {
            frontPid = frontPid->next;
        }
        free(temp);
        return rval;
    }
}
void DisplayPidQueue()
{
    struct pidNode *temp = frontPid;
    printf("\nqueue:");
    while (temp != NULL)
    {
        printf("%d,", temp->pid);
        temp = temp->next;
    }
    printf("\n");
}
void RemoveFromPidQueue(int pid)
{
    if (frontPid == NULL)
    {
        return;
    }
    else if (frontPid->pid == pid)
    {
        DequeuePid();
    }
    else
    {
        struct pidNode *temp;
        temp = frontPid;
        while (temp->next != NULL && temp->next->pid != pid)
        {
            temp = temp->next;
        }
        if (temp->next == NULL)
        {
            return;
        }
        else
        {
            struct pidNode *tempv2;
            tempv2 = temp->next->next;
            free(temp->next);
            temp->next = tempv2;
            while(temp->next != NULL){
                temp = temp->next;
            }
            rearPid = temp;
        }
    }
}
int FrontOfPidQueue()
{
    if (frontPid == NULL)
    {

        return -1;
    }
    else
    {
        return frontPid->pid;
    }
}
void EnqueuePid(int pid)
{
    struct pidNode *nptr = malloc(sizeof(struct pidNode));
    nptr->pid = pid;
    nptr->next = NULL;
    if (rearPid == NULL)
    {
        frontPid = nptr;
        rearPid = nptr;
    }
    else
    {
        rearPid->next = nptr;
        rearPid = rearPid->next;
    }
}
bool SearchInPidQueue(int pid)
{
    if (frontPid == NULL)
    {
        return false;
    }
    else
    {
        struct pidNode *temp;
        temp = frontPid;
        while (temp != NULL)
        {
            if (temp->pid == pid)
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

#endif