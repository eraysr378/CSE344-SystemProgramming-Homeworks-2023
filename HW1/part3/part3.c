#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include<sys/types.h>
#include <unistd.h>


int main(){

    int fd1 = open("file1.txt", O_RDONLY | O_WRONLY | O_APPEND | O_CREAT,0666);
    int fd2 = open("file2.txt", O_RDONLY | O_WRONLY | O_APPEND | O_CREAT,0666);

    printf("fd1:%d, fd2:%d\n",fd1,fd2);
    // set offset values differently
    lseek(fd1,5,SEEK_SET);
    lseek(fd2,3,SEEK_SET);
    off_t fd1_off = lseek(fd1, 0, SEEK_CUR);
    off_t fd2_off = lseek(fd2, 0, SEEK_CUR);

    printf("fd1_offset:%ld,  fd2_offset:%ld\n",fd1_off,fd2_off);

    write(fd1,(const char*)"1",1); //written to file1.txt
    write(fd2,(const char*)"0",1); // written to file2.txt

    dup2(fd1,fd2);

    printf("After dup2(fd1,fd2)\n");
    fd1_off = lseek(fd1, 0, SEEK_CUR);
    fd2_off = lseek(fd2, 0, SEEK_CUR);
    printf("fd1_offset:%ld,  fd2_offset:%ld\n",fd1_off,fd2_off);

    write(fd1,(const char*)"2",1); // written to file1.txt
    write(fd2,(const char*)"3",1); // written to file1.txt
    
    fd1_off = lseek(fd1, 0, SEEK_CUR);
    fd2_off = lseek(fd2, 0, SEEK_CUR);
    printf("fd1_offset:%ld,  fd2_offset:%ld\n",fd1_off,fd2_off);

    close(fd1);
    close(fd2);
}