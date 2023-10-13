#ifndef GLOBALS_H
#define GLOBALS_H

#define MAX_OPEN_FILE_COUNT 1000

int BUFFER_SIZE;                    // size of buffer given as command line argument
int POOL_SIZE;                      // size of the consumer thread pool given as command line argument
FileInfoBuffer *Buffer = NULL;      // buffer to store file informations
int Done = 0;                       // done flag, producer will make this 1 when it is done
int KillSignal = 0;                 // when kill signal received this variable will be 1 and threads will be closed
int TotalByteCopied = 0;            // total amount of copied bytes
int FilesSucceed = 0;               // number of regular files that succeed to copy
int FilesFailed = 0;                // number of regular files that failed to copy
int FifoSucceed = 0;                // number of fifo files that succeed to copy
int FifoFailed = 0;                 // number of fifo files that failed to copy
int DirsSucceed = 0;                // number of directories that succeed to copy
int DirsFailed = 0;                 // number of directories that failed to copy
int OpenedFileCount = 0;            // number of opened and not closed files used to not exceed the fd limit
pthread_mutex_t Mutex_m;            // mutex to synchronize producer and consumers (also consumers between themselves)
pthread_mutex_t Mutex_write_stdout; // mutex to control the printf
pthread_mutex_t Mutex_open_file;
pthread_cond_t Cond_empty;          // producer waits for this cond variable to fill the buffer
pthread_cond_t Cond_full;           // consumer waits for this cond variable to empty the buffer
pthread_cond_t Cond_fd_count;           // consumer waits for this cond variable to empty the buffer
pthread_t ProducerThread;           // only one producer thread
pthread_t *ConsumerThreads = NULL;  // consumer thread amount can be more than one

#endif