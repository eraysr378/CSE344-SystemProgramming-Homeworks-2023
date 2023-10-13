#ifndef FILE_FUNCS_H
#define FILE_FUNCS_H
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mytypes.h"
#include <stdbool.h>
void CreateRootDir(File **head, char root[])
{
    File *nf = (File *)malloc(sizeof(File));
    strcpy(nf->name, root);
    strcpy(nf->path, root);
    nf->type = 4;
    nf->ownerId = -1;
    nf->mTime = 0;
    for (int i = 0; i < MAX_FILE; i++)
    {
        nf->files[i] = NULL;
    }
    *head = nf;
}

// insertion at the beginning
void AppendFile(File **head, char filePath[], int ownerId, long mTime, int type)
{
    char *currentFile;
    char *savePtr;
    char currentFilePath[1024];
    char filePathCpy[1024];
    strcpy(filePathCpy, filePath);

    if (*head == NULL)
    {
        printf("Create ROot dir\n");
        exit(0);
    }

    File *file = *head;
    currentFile = strtok_r(filePathCpy, "/", &savePtr);
    strcpy(currentFilePath, currentFile);

    while (file != NULL)
    {

        int i = 0;
        for (; i < MAX_FILE; i++)
        {
            if (file->files[i] != NULL)
            {
                // printf("here %s,%s\n", currentFile, file->name);
                if(strcmp(filePath, file->files[i]->path) == 0){
                    return;
                }

                if (strcmp(currentFile, file->files[i]->name) == 0)
                {
                    file = file->files[i];
                    break;
                }
            }
        }
        if (i == MAX_FILE)
        {
            for (i = 0; i < MAX_FILE; i++)
            {
                if (file->files[i] == NULL)
                {
                    File *nf = (File *)malloc(sizeof(File));
                    strcpy(nf->name, currentFile);
                    strcpy(nf->path, currentFilePath);
                    if (strcmp(nf->path, filePath) != 0)
                    {
                        nf->type = 4; // directory
                    }
                    else
                    {
                        nf->type = type; // file or directory
                        nf->ownerId = ownerId;
                        nf->mTime = mTime;
                    }
                    nf->marked = false;
                    nf->deleted = false;
                    for (int i = 0; i < MAX_FILE; i++)
                    {
                        nf->files[i] = NULL;
                    }
                    // printf("inside%s, file created:%s\n", file->name, nf->name);

                    file->files[i] = nf;
                    file = file->files[i];

                    break;
                }
            }
        }
        currentFile = strtok_r(NULL, "/", &savePtr);
        if (currentFile == NULL)
            break;
        strcat(currentFilePath, "/");
        strcat(currentFilePath, currentFile);
    }
}
File *SearchInFiles(File **head, char filePath[])
{
    char *currentFile, *savePtr;
    char currentFilePath[1024], filePathCpy[1024];

    strcpy(filePathCpy, filePath);
    File *file = *head;
    currentFile = strtok_r(filePathCpy, "/", &savePtr);
    strcpy(currentFilePath, currentFile);

    while (file != NULL)
    {
        int i = 0;
        for (; i < MAX_FILE; i++)
        {
            if (file->files[i] != NULL)
            {
                if (strcmp(currentFile, file->files[i]->name) == 0)
                {
                    file = file->files[i];
                    break;
                }
            }
        }
        if (i == MAX_FILE)
            return NULL;

        currentFile = strtok_r(NULL, "/", &savePtr);
        if (currentFile == NULL)
            return file;

        strcat(currentFilePath, "/");
        strcat(currentFilePath, currentFile);
    }
    return NULL;
}
// filepath is relative path
File *DeleteGivenFile(File **head, char filePath[])
{
    char *currentFile, *savePtr;
    char currentFilePath[1024], filePathCpy[1024];

    strcpy(filePathCpy, filePath);
    File *file = *head;
    currentFile = strtok_r(filePathCpy, "/", &savePtr);
    strcpy(currentFilePath, currentFile);

    while (file != NULL)
    {
        int i = 0;
        for (; i < MAX_FILE; i++)
        {
            if (file->files[i] != NULL)
            {
                if (strcmp(filePath, file->files[i]->path) == 0)
                {
                    File *curFile = file->files[i];
                    // if it is a directory, free inner files
                    for (int j = 0; j < MAX_FILE; j++)
                    {
                        if (curFile->files[j] != NULL)
                            DeleteGivenFile(head, curFile->files[j]->path);
                    }
                    free(file->files[i]);

                    file->files[i] = NULL;
                    return NULL;
                }
                else if (strcmp(currentFile, file->files[i]->name) == 0)
                {
                    file = file->files[i];
                    currentFile = strtok_r(NULL, "/", &savePtr);
                    if (currentFile == NULL)
                        return NULL;

                    strcat(currentFilePath, "/");
                    strcat(currentFilePath, currentFile);
                    break;
                }
            }
        }
        if (i == MAX_FILE)
        {
            printf("cant delete file does not exist %s\n", filePath);
            return NULL;
        }
    }
    return NULL;
}
void PrintFileTree(File *root, int indent)
{
    if (root != NULL)
    {
        for (int i = 0; i < indent; i++)
        {
            printf(" ");
        }
        if (root->type == 4)
        {
            if (root->deleted)
                printf("%s (dir deleted)\n", root->name);
            else
                printf("%s (dir)\n", root->path);
        }

        else
        {
            if (root->deleted)
                printf("%s (file deleted)\n", root->name);
            else
                printf("%s (file)\n", root->path);
        }
        if (root->type == 4)
        {
            for (int i = 0; i < MAX_FILE; i++)
            {
                PrintFileTree(root->files[i], indent + 3);
            }
        }
    }
}
File *GetUnmarkedFile(File **head)
{
    File *file = *head;
    if (file != NULL)
    {
        for (int i = 0; i < MAX_FILE; i++)
        {
            if (file->files[i] != NULL)
            {
                if (file->files[i]->marked == false)
                    return file->files[i];
                else
                {
                    File *f = GetUnmarkedFile(&(file->files[i]));
                    if (f != NULL)
                        return f;
                }
            }
        }
    }
    return NULL;
}
File *UnmarkAll(File **head)
{
    File *file = *head;
    if (file != NULL)
    {
        for (int i = 0; i < MAX_FILE; i++)
        {
            if (file->files[i] != NULL)
            {
                file->files[i]->marked = false;
                UnmarkAll(&(file->files[i]));
            }
        }
    }
}

void CleanFiles(File **head)
{
    if ((*head) != NULL)
    {
        for (int i = 0; i < MAX_FILE; i++)
        {
            if ((*head)->files[i] != NULL)
            {
                DeleteGivenFile(head, (*head)->files[i]->path);
            }
        }
    }
}
File *SearchInFilesByOwnerId(File **head, int ownerId)
{
    File *file = *head;
    if (file != NULL)
    {
        for (int i = 0; i < MAX_FILE; i++)
        {
            if (file->files[i] != NULL)
            {
                if (file->files[i]->ownerId == ownerId)
                    return file->files[i];
                else
                {
                    File *f = SearchInFilesByOwnerId(&(file->files[i]), ownerId);
                    if (f != NULL && f->ownerId == ownerId)
                        return f;
                }
            }
        }
    }
    return NULL;
}

void* CloneFiles(File **head,File **clone)
{
    UnmarkAll(head);
    File *file;
    while (1)
    {
        file = GetUnmarkedFile(head);
        if (file == NULL)
            break;
        else
            file->marked = true;
        printf("file:%s\n",file->path);
        AppendFile(clone,file->path,file->ownerId,file->mTime,file->type);
        SearchInFiles(clone,file->path)->deleted = file->deleted;
    }
}
#endif
