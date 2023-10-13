#ifndef GLOBALS_H
#define GLOBALS_H
#include <stdio.h>
#include <stdbool.h>
#include "mytypes.h"

#define MAX 1024
#define SA struct sockaddr
File* listLastSync;
File* listUpdated;
int POOL_SIZE;
int PORT;
bool SyncStarted = false;
int ConnectionRequests = 0;
int SyncDoneCount = 0;
int ClientIdCount = 2;
int SyncClientCount = 0;
int SyncCheckCount = 0;
int ReadyToSyncCount = 0;
char SV_DIR[256];
int ConnectionFd = -1;
int ConnectedClientNum = 0;
bool ConnectionSync = false; // if a client is connecting, synchronization should be stopped till the client is synchronized
pthread_mutex_t Mutex_connFd; // mutex to control the printf
pthread_mutex_t Mutex_sync_or_connect; // mutex to client synchronization at start
pthread_mutex_t Mutex_fileUpdated; // used when changing file list
pthread_mutex_t Mutex_fileLastSync; // used when changing file list
pthread_mutex_t Mutex_sync_threads; // when updating server, wait all threads to execute

pthread_cond_t Cond_fdReady;  // producer waits for this cond variable to fill the buffer
pthread_cond_t Cond_fdWait;   // consumer waits for this cond variable to empty the buffer
pthread_cond_t Cond_sync_threads;
pthread_cond_t Cond_connect_client;
pthread_cond_t Cond_sync_or_connect;
#endif