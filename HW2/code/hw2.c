#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>
pid_t child_pid = -1; // global child pid to handle signals
// returns the index of the given character in given string
int get_index(char* string, char c) {
    char* e = strchr(string, c);
    if (e == NULL) {
        return -1;
    }
    return (int)(e - string);
}
int check_close(int fd) {
    int r = close(fd);
    if (r == -1) {
        perror("close");
        fprintf(stderr, "close(\"%d\") failed! \n", fd);
    }
    return r;
}
// move oldfd to newfd
int redirect(int oldfd, int newfd) {
    if (oldfd != newfd) {
        if (dup2(oldfd, newfd) != -1) {
            return check_close(oldfd);
        }
        else
        {
            perror("dup2");
            fprintf(stderr, "dup2(%d,%d) failed! Make sure file exists. \n", oldfd, newfd);
            return -1;
        }
    }
}

int runCommand(char* command, int read_fd, int write_fd) {
    // child will read from read_fd instead of stdin, if redirection fails kill the child
    if (redirect(read_fd, STDIN_FILENO) == -1) {
        kill(getpid(), SIGKILL);
    }   
    // child will write to write_fd instead of stdout, if redirection fails kill the child 
    if (redirect(write_fd, STDOUT_FILENO) == -1) {
        kill(getpid(), SIGKILL);
    } 
    execl("/bin/sh", "sh", "-c", command, (char*)NULL);

    // The code below will run only if execl fails
    perror("execl");
    fprintf(stderr, "execl failed! \n");
    return -1;
}

// handlers
void SIGINT_handler(int sig)
{
    if (child_pid > 0) {
        fprintf(stderr, "\nSIGINT received, child is killed pid=%d\n", child_pid);
        kill(child_pid, SIGKILL);
        child_pid = -1;
    }
    else {
        fprintf(stderr, "\nSIGINT received, there is no child to kill\n$ ");
    }
}
void SIGTERM_handler(int sig)
{
    if (child_pid > 0) {
        fprintf(stderr, "\nSIGTERM received, child is killed pid=%d\n", child_pid);
        kill(child_pid, SIGKILL);
        child_pid = -1;
    }
    else {
        fprintf(stderr, "\nSIGTERM received, there is no child to kill\n$ ");
    }
}
void SIGQUIT_handler(int sig)
{
    if (child_pid > 0) {
        fprintf(stderr, "\nSIGQUIT received, child is killed pid=%d\n", child_pid);
        kill(child_pid, SIGKILL);
        child_pid = -1;
    }
    else {
        fprintf(stderr, "\nSIGQUIT received, there is no child to kill\n$ ");
    }
}
void SIGHUP_handler(int sig)
{
    if (child_pid > 0) {
        fprintf(stderr, "\nSIGHUP received, child is killed pid=%d\n", child_pid);
        kill(child_pid, SIGKILL);
        child_pid = -1;
    }
    else {
        fprintf(stderr, "\nSIGHUP received, there is no child to kill\n$ ");
    }
}
// this function is useless because SIGKILL cannot be handled or blocked
void SIGKILL_handler(int sig) {
    fprintf(stderr, "\nSIGKILL cannot be handled or blocked therefore this function is useless\n");
}

int main(void) {
    printf("Welcome to the terminal\nThe files you give as input and output should not include whitespace\nIf they include then whitespaces will be removed.\n");
    time_t t;

    char buf[1024];
    int saved_stdin = dup(STDIN_FILENO);
    int saved_stdout = dup(STDOUT_FILENO);
    char commandLine[1024];
    signal(SIGINT, SIGINT_handler);
    signal(SIGTERM, SIGTERM_handler);
    signal(SIGQUIT, SIGQUIT_handler);
    signal(SIGHUP, SIGHUP_handler);
    signal(SIGTSTP, SIG_IGN); // we do not want to stop
    signal(SIGKILL, SIGKILL_handler);

    char delim[] = "|";
    while (1) {
        dup2(saved_stdin, 0); // make sure stdin is not changed
        fflush(stdin);

        // get commands from the terminal
        char* commands[20];
        int arguments = 0;
        printf("$ "); // give it a terminal look
        fgets(commandLine, 1024, stdin);
        // parse the commands one by one
        char* ptr = strtok(commandLine, delim);
        while (ptr != NULL && arguments <= 20)
        {
            commands[arguments++] = ptr;
            ptr = strtok(NULL, delim);
        }
        if (arguments > 20) {
            printf("\nAt most, 20 commands can be run at once!\n");
            continue;
        }
        // check if the user wants to quit
        if (strcmp(commandLine, ":q\n") == 0)
        {
            printf("\nexit\n");
            exit(0);
        }
        // open the log file to write the logs
        time(&t);     
        int logfd = open(ctime(&t), O_RDONLY | O_WRONLY | O_APPEND | O_CREAT, 0666);
        int read_fd = STDIN_FILENO; // read the first command from stdin, later it changes
        for (int i = 0; i < arguments; ++i) {
            fflush(0);
            int fd[2]; // write and read ends of the pipe
            pid_t pid;
            if (pipe(fd) == -1) {
                perror("pipe");
                fprintf(stderr, "Error occured when pipe is being created, try again. \n");
                break;
            }
            else if ((pid = fork()) == -1) {
                perror("fork");
                fprintf(stderr, "Fork failed, try again. \n");
                break;
            }
            // child
            else if (pid == 0) {
                if (check_close(fd[0]) == -1)// child will write, so we can close read end
                    break;
                char pidstr[256];// a buffer big enough to hold the information about the child
                sprintf(pidstr, "My pid: %d, My command: %s \n", getpid(), commands[i]);
                // prevent nulls in the file
                for (int k = 0; k < 256; k++) {
                    if (pidstr[k] == '\0') {
                        break;
                    }
                    else {
                        write(logfd, (pidstr + k), 1);
                    }
                }
                if (check_close(logfd) == -1)
                    break;
                // input output redirection check section
                if (strstr(commands[i], "<") != NULL && strstr(commands[i], ">") != NULL)
                {
                    // parse the command into parts command < inputfile > outputfile
                    int index1 = get_index(commands[i], '<');
                    int index2 = get_index(commands[i], '>');
                    int comsize = get_index(commands[i], '\0');
                    char com[index1 + 1];
                    char infile[index2 - index1];
                    char outfile[comsize - index2];
                    for (int k = 0; k < index1;k++) {
                        com[k] = commands[i][k];
                    }
                    com[index1] = '\0';// make sure command ends with null
                    // remove whitespaces from the input file name to prevent unwanted situations
                    int j = 0;
                    for (int k = 0;k < index2 - index1 - 1;j++, k++) {
                        infile[j] = commands[i][k + index1 + 1];
                        if (infile[j] == ' ' || infile[j] == '\n') {
                            j--;
                        }
                    }
                    infile[j] = '\0';// make sure file name ends with null
                    // remove whitespaces and \n character from the output file name to prevent unwanted situations
                    j = 0;
                    for (int k = 0;k < comsize - index2 - 1;j++, k++) {
                        outfile[j] = commands[i][k + index2 + 1];
                        if (outfile[j] == ' ' || outfile[j] == '\n') {
                            j--;
                        }
                    }
                    outfile[j] = '\0';// make sure file name ends with null
                    read_fd = open(infile, O_RDONLY, 0); // open input file to get input
                    int fd_out = open(outfile, O_RDONLY | O_WRONLY | O_APPEND | O_CREAT, 0666); // open output file to write output
                    if (runCommand(com, read_fd, fd_out) == -1)
                        break;

                }
                else if (strstr(commands[i], "<") != NULL)
                {
                    // parse the command into parts command < inputfile
                    int index1 = get_index(commands[i], '<');
                    int comsize = get_index(commands[i], '\0');
                    char com[index1 + 1];
                    char infile[comsize - index1];
                    infile[comsize - index1 - 1] = '\0';
                    for (int k = 0; k < index1 ; k++) {
                        com[k] = commands[i][k];
                    }
                    com[index1] = '\0';
                    // remove whitespaces and \n character from the input filename to prevent unwanted situations
                    int j = 0;
                    for (int k = 0;k < comsize - index1 - 1;j++, k++) {
                        infile[j] = commands[i][k + index1 + 1];
                        if (infile[j] == ' ' || infile[j] == '\n') {
                            j--;
                        }
                    }
                    infile[j] = '\0';// make sure file name ends with null
                    read_fd = open(infile, O_RDONLY, 0);
                    if (i == arguments - 1) {
                        if (runCommand(com, read_fd, saved_stdout) == -1)
                            break;
                    }
                    else {
                        if (runCommand(com, read_fd, fd[1]) == -1)
                            break;
                    }
                }
                else if (strstr(commands[i], ">") != NULL)
                {
                    // parse the command into parts command > outputfile
                    int index1 = get_index(commands[i], '>');
                    int comsize = get_index(commands[i], '\0');
                    char com[index1 + 1];
                    char outfile[comsize - index1];
                    outfile[comsize - index1 - 1] = '\0';
                    for (int k = 0; k < index1; k++) {
                        com[k] = commands[i][k];
                    }
                    com[index1] = '\0';
                    // remove whitespaces and \n character from the output filename to prevent unwanted situations
                    int j = 0;
                    for (int k = 0;k < comsize - index1 - 1;j++, k++) {
                        outfile[j] = commands[i][k + index1 + 1];
                        if (outfile[j] == ' ' || outfile[j] == '\n') {
                            j--;
                        }
                    }
                    outfile[j] = '\0';
                    dup2(saved_stdin,0); // copy stdin back to its place, otherwise fd_out gets 0 and error occurs
                    int fd_out = open(outfile, O_RDONLY | O_WRONLY | O_APPEND | O_CREAT, 0666);

                    if (runCommand(com, read_fd, fd_out) == -1)
                        break;
                }
                // if its time to run the last command, then print the result to the terminal
                else if (i == arguments - 1) {
                    if (runCommand(commands[i], read_fd, saved_stdout) == -1)
                        break;
                }
                else if (runCommand(commands[i], read_fd, fd[1]) == -1)/* $ command < in > fd[1] */
                    break;
            }
            // parent
            else {
                int status;
                child_pid = pid; // set global child_pid to handle signals
                waitpid(pid, &status, 0);
                // after the child is executed, there is no child left
                child_pid = -1;
                if (check_close(fd[1]) == -1)// close unused write end of the pipe
                    break;
                if (check_close(read_fd) == -1)// close unused read end of the previous pipe 
                    break;
                read_fd = fd[0]; // the next command reads from here 
            }
        }
        check_close(logfd);// close just in case.
    }

}

// child test command pgrep -P $pid 
// detailed version ps -fp `pgrep -P $pid`
//valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose   ./out