#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>


int main(int argc, char* argv[]){
   
    char* filename = argv[1];
    int numOfBytes = atoi(argv[2]);
    char* byte = "0";
    int fd;

    // error handling
    if(argc != 3 && argc != 4){
        fprintf(stderr,"Only 2 or 3 arguments allowed.\n");
        exit(1);
    }
    if(numOfBytes <= 0){
        fprintf(stderr,"Number of bytes should be bigger than zero.\n");
        exit(1);
    }
    if(argc == 3){
        fd = open(filename,  O_WRONLY | O_CREAT | O_APPEND,0666);
        while (numOfBytes > 0)
        {      
            write(fd,byte,1);
            numOfBytes--;
        }
    }else{
        fd = open(filename,  O_WRONLY | O_CREAT,0666);
        while (numOfBytes > 0)
        {      
            lseek(fd,0,SEEK_END);
            write(fd,byte,1);
            numOfBytes--;
        }
    }
    printf("Successful\n");
    close(fd);
    return 0;
}