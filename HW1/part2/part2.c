#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include<sys/types.h>
#include <unistd.h>

int newdup(int oldfd){
    return fcntl (oldfd, F_DUPFD, 0); // duplicates old file descriptor using the lowest numbered available file descriptor 
}

int newdup2(int oldfd, int newfd){
    // if old file descriptor does not exist, set errno accordingly and return -1
    if(fcntl(oldfd,F_GETFL) == -1){
        errno = EBADF;
        return -1;
    }
    // if old file descriptor and new file descriptor are the same, return one of them
    if (oldfd == newfd) {
        return newfd;
    }
    // otherwise close the new file descriptor and duplicate old file descriptor
    else {
        close(newfd);
        return fcntl(oldfd, F_DUPFD, newfd);
    }

}
int main(int args, char* argv[]){
    char *zero = "0";
    char *one = "1";

    int fd = newdup(0);// sets fd to the lowest available descriptor
    printf("fd after newdup(0):%d\n", fd);
    close(fd);
  
    int fd1 = open("file1.txt", O_RDONLY | O_WRONLY | O_APPEND | O_CREAT,0666);
    int fd2 = open("file2.txt", O_RDONLY | O_WRONLY | O_APPEND | O_CREAT,0666);
    int fd5 = newdup(0); // sets fd5 to the lowest available descriptor
    int fd3;
    printf("fd1 created:%d\n",fd1);
    printf("fd2 created:%d\n",fd2);
    printf("fd5 after newdup:%d\n",fd5);

    write(fd2,zero,1); // writes to file2.txt
    fd3 = newdup2(fd1, fd2); // after this line, all write operations on fd2 are written into file1.txt (fd1)
    write(fd2,one,1); // writes to file1.txt
    printf("fd3 after newdup2(fd1:%d,fd2:%d):%d\n",fd1,fd2,fd3);
    perror("error check");
    
    int fd4 = newdup2(fd2, 15); // invalid new file descriptor given, error should be taken
    printf("fd4 after newdup2(fd2:%d,15):%d\n",fd2, fd4);
    perror("error check");
    
    close(fd1);
    close(fd2);
    close(fd3);
    close(fd4);
    close(fd5);


    return 0;
}

