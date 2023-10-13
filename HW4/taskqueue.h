
#ifndef TASKQUEUE_H
#define TASKQUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <memory.h>
#include "mytypes.h"
#include <unistd.h>

struct taskNode *frontTask = NULL;
struct taskNode *rearTask = NULL;
void DisplayTaskQueue()
{
    struct taskNode *temp = frontTask;
    printf("\nqueue:");
    while (temp != NULL)
    {
        printf("%d,%s\n", temp->task.pid, temp->task.request);
        temp = temp->next;
    }
}
Task FrontOfTaskQueue()
{
    if (frontTask == NULL)
    {
        Task task;
        task.pid = -1;
        return task;
    }
    else
    {
        return frontTask->task;
    }
}
void EnqueueTask(Task task)
{
    struct taskNode *nptr = malloc(sizeof(struct taskNode));
    nptr->task = task;
    nptr->next = NULL;
    if (rearTask == NULL)
    {
        frontTask = nptr;
        rearTask = nptr;
    }
    else
    {
        rearTask->next = nptr;
        rearTask = rearTask->next;
    }
}
bool SearchInTaskQueue(Task task)
{
    if (frontTask == NULL)
    {
        return false;
    }
    else
    {
        struct taskNode *temp;
        temp = frontTask;
        while (temp != NULL)
        {
            if (temp->task.pid == task.pid)
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
Task DequeueTask()
{
    if (frontTask == NULL)
    {
        Task task;
        task.pid = -1;
        return task;
    }
    else
    {
        struct taskNode *temp;
        Task rval = frontTask->task;
        temp = frontTask;
        if (frontTask->next == NULL)
        {
            frontTask = NULL;
            rearTask = NULL;
        }
        else
        {
            frontTask = frontTask->next;
        }
        free(temp);
        return rval;
    }
}

#endif