#ifndef MYCONST_H
#define MYCONST_H

#define MESSAGE_LEN 512
#define MAX_CHAR_IN_LINE 512

#define SERVER_FIFO_TEMPLATE "/tmp/seqnum_sv_%ld"
#define SERVER_FIFO_NAME_LEN (sizeof(SERVER_FIFO_TEMPLATE)+20)
#define CLIENT_FIFO_TEMPLATE "/tmp/seqnum_cl_%ld"
#define CLIENT_FIFO_NAME_LEN (sizeof(CLIENT_FIFO_TEMPLATE)+20)


#endif