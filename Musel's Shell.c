#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#define ARGS_COUNT 8
#define COMMANDS_COUNT 8
#define BUF_SIZE 128

#define ERROR_EMPTY(errorStr) do {fprintf(stderr,"%sPlease check and try again :D\n",errorStr); return false;} while(0)
#define ERROR_IOFILE do {fprintf(stderr,"\033[1;31mError:\033[0m I/O file format error! \nPlease check and try again :D\n"); return false;} while(0)
#define ERROR_STR do {fprintf(stderr,"\033[1;31mError:\033[0m Your str miss \' or \" ! \nPlease check and try again :D\n"); return false;} while(0)
#define ERROR_FORK do {fprintf(stderr,"\033[1;31mError:\033[0m Failed to fork process!"); return; } while(0)
#define ERROR_OPEN do {fprintf(stderr,"\033[1;31mError:\033[0m I/O file format error! \nPlease check and try again :D\n");exit(0); return;} while(0)
#define ERROR_EXECUTE(errorCmd) do {fprintf(stderr, "\033[1;31mError:\033[0m Failed to execute cmd %s!\n", errorCmd);exit(0); return; } while(0)
#define ERROR_EXIT do {fprintf(stderr, "\033[1;31mError:\033[0m Failed to exit with status %d!\n", WEXITSTATUS(status)); return; } while(0)

char buf[32];
char *start;
char **args;
int commandsCount = 0;
int status;

typedef struct {
    char *read;
    char *write;
    char *overwrite;
    char **args;
    char *cmd;
} Command;

bool fetchFileName(char **bufAddr, char **cmdFileAddr)
{
    start = (*bufAddr);
    char *buf = *bufAddr;
    while ((*buf != '\n') && (!isspace(*buf)) && (*buf != '|') && (*buf != '<') && (*buf != '>')) buf = (++(*bufAddr));
    if (buf == start) return false;

    (*cmdFileAddr) = (char *)malloc(sizeof(char) * (buf-start+1));
    memcpy((*cmdFileAddr), start, sizeof(char) * (buf-start));
    (*cmdFileAddr)[(buf-start)] = '\0';
    return true;
}

void createCommand(Command *command) {
    command->read = command->overwrite = command->write = NULL;
    command->args = NULL;
    command->cmd = NULL;
}

bool splitCommands(char *buf, Command *commands) {
    int waitCommand = 1;
    createCommand(commands);
    args = commands->args = (char **)malloc(sizeof(char *) * ARGS_COUNT);
    

    while (1) {
        switch(*buf) {
            case ' ' : 
                while ((*buf != '\n') && isspace(*buf)) {++buf;}
                break;

            case '\n': 
                if (waitCommand) ERROR_EMPTY("\033[1;31mError:\033[0m Next command shouldn't be empty!\n");  
                *args = (char *)malloc(sizeof(char));
                *args = 0;
                commands++;
                commands->cmd=NULL;
                return true;
                
            case '|' : 
                if (waitCommand) ERROR_EMPTY("\033[1;31mError:\033[0m Pipe should be used after a command!\n");
                waitCommand = 1;
                buf++; 
                *args = (char *)malloc(sizeof(char));
                *args = 0;
                commandsCount++;
                commands++;
                createCommand(commands);
                args = commands->args = (char **)malloc(sizeof(char *) * ARGS_COUNT);
                break; 

            case '<' : 
                if (waitCommand) ERROR_EMPTY("\033[1;31mError:\033[0m I/O redirection should be used after a command!\n");
                buf++;
                while ((*buf != '\n') && isspace(*buf)) ++buf;
                if(fetchFileName(&buf, &(commands->read))==false) ERROR_IOFILE;
                break;

            case '>' : 
                if (waitCommand) ERROR_EMPTY("\033[1;31mError:\033[0m I/O redirection should be used after a command!\n");
                buf++;
                while ((*buf != '\n') && isspace(*buf)) ++buf;
                if (*(buf) != '>') { if(fetchFileName(&buf, &(commands->write))==false) ERROR_IOFILE; break; }
                buf++;
		        while ((*buf != '\n') && isspace(*buf)) {++buf;}
                if(fetchFileName(&buf, &(commands->overwrite))==false) ERROR_IOFILE; break;

            case '\'': 
            case '\"':
                if (waitCommand) ERROR_EMPTY("\033[1;31mError:\033[0m String parameter should be used after a command!\n");
                start = buf++;
                while((*buf != '\n') && (*buf != *start)) { buf++; }
                if ((*buf == '\n')||((buf-start-1)<0)) ERROR_STR;
                (*args) = (char *)malloc(sizeof(char) * (buf-start));
                memcpy(*args, start+1, sizeof(char) * (buf-start-1));
                (*args)[(buf-start-1)] = '\0';
                args++;
                buf++;
                break;

            default :
                start = buf;
                while ((*buf != '\n') && (!isspace(*buf)) && (*buf != '|') && (*buf != '<') && (*buf != '>')) {
				++buf;}
                
                (*args) = (char *)malloc(sizeof(char) * (buf-start+1));
                char *addr = *args;
                args++;

                memcpy(addr, start, sizeof(char) * (buf-start));
                addr[(buf-start)] = '\0';

                if (waitCommand) {
                    waitCommand = 0;
                    
                    commands->cmd = (char *)malloc(sizeof(char) * (buf-start+1));
                    addr = commands->cmd;
                    memcpy(addr, start, sizeof(char) * (buf-start));
                    addr[(buf-start)] = '\0';
                }
                break;
        }
    }
}

void forkToExecute(Command *command, int fd_in, int fd_out) {
    if (strcmp(command->cmd, "exit") == 0) {free(command);exit(0);}
    else if (strcmp(command->cmd, "cd") == 0) {
        if (chdir(command->args[1]) != 0) {
            fprintf(stderr, "\033[1;31mError:\033[0m Cannot cd :%s\n", command->args[1]);
        }
        return;
    }

    pid_t pid = fork();
    if (pid < 0) ERROR_FORK;
    else if (pid == 0) {
        if (command->read) {
            int in = open(command->read, O_RDONLY, 0666);
	    if (in<0) ERROR_OPEN;
            dup2(in, 0);
        } else if (fd_in > 0) {
            dup2(fd_in, 0);
        }

        if (command->write) {
            int out = open(command->write, O_RDWR | O_CREAT, 0666);
            dup2(out, 1);
        } else if (command->overwrite) {
            int append = open(command->overwrite, O_WRONLY | O_CREAT | O_APPEND, 0666);
            dup2(append, 1);
        } else if (fd_out > 0) {
            dup2(fd_out, 1);
        }

        if((status = execvp(command->cmd, command->args)) < 0) ERROR_EXECUTE(command->cmd);    
    } else {
        wait(&status);
        if (!(WIFEXITED(status))) ERROR_EXIT;
    }
}

void freeCommand(Command *command) {
    free(command->read);
    free(command->write);
    free(command->overwrite);
    free(command->cmd);
    for (char **args = command->args; *args; ++args) free(*args);
    free(command->args);
}

void executeCommands(Command *commands) {
    if (commandsCount == 1) {
        forkToExecute(commands,-1,-1);
        freeCommand(commands);
    }
    else if (commandsCount == 2) {
        int fd[2];
        pipe(fd);
        forkToExecute(commands,-1,fd[1]);
        close(fd[1]);
        freeCommand(commands++);
        forkToExecute(commands,fd[0],-1);
        close(fd[0]);
        freeCommand(commands);
    } else {
        int *pipes[2];
        pipes[0]=(int *)malloc(sizeof(int)*2);
        pipes[1]=(int *)malloc(sizeof(int)*2);
        int newPoint = 0;
        pipe(pipes[newPoint]);
        forkToExecute(commands,-1,(pipes[newPoint])[1]);
        close((pipes[newPoint])[1]);
        freeCommand(commands++);

        for (int i = 1; i < (commandsCount - 1); ++i) {
            newPoint = 1-newPoint;
            
            pipe(pipes[newPoint]);
            forkToExecute(commands,(pipes[1-newPoint])[0],(pipes[newPoint])[1]);
            close((pipes[1-newPoint])[0]);
            close((pipes[newPoint])[1]);
            freeCommand(commands++);
        }

        forkToExecute(commands,(pipes[newPoint])[0],-1);
        close((pipes[newPoint])[0]);
        freeCommand(commands);
    }
    return;
}

void print_current_directory(){
    char *path = NULL;
    path = getcwd(NULL, 0);

    printf("\033[1;34m%s\033[0m%s\033[1;36m%s\033[0m $ ","Musel's shell",":", path);
    free(path);
}
    
int main () {
	printf("\033[1;33m-------------------------------------------------\n");
	printf("\033[1;33m|   	    Welcome to Musel's shell :)		|\n");
	printf("\033[1;33m-------------------------------------------------\n");
    while (1) {
        print_current_directory();

        if (fgets(buf, BUF_SIZE, stdin) != NULL) {
            Command *commands = malloc(COMMANDS_COUNT * sizeof(Command));
            commandsCount = 1;
            if (splitCommands(buf, commands)) executeCommands(commands);
            free(commands);
        }
    }
}
