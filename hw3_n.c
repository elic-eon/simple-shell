#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <termios.h>

#define TOK_DELIM " \t\r\n\a"

static pid_t SH_PID;
static pid_t SH_PGID;
static struct termios SH_TMODES;
pid_t child_pid;
struct sigaction act_int;
struct sigaction act_chld;
int no_reprint;

void sigchld_handler (int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}
void sigint_handler (int sig) {
    if (kill(child_pid, SIGTERM) == 0) {
        printf("kill: %d\n", child_pid);
        no_reprint = 1;
    }
    else {
        printf(" int \n");
    }
}

void initialize(void);
void sh_loop(void);
void printPrompt(void);
void toArgv(char *buffer, char **argv);
int lunchCmd(char *tokens[]);
int isBackground(char *tokens[], int l);
void lunchProg(char *tokens[], int isBackground);

int main() {
    initialize();
    sh_loop();
    return 0;
}

void initialize(void) {
    child_pid = -10;
    no_reprint = 0;
    SH_PID = getpid();

    while (tcgetpgrp(STDIN_FILENO) != (SH_PGID = getpgrp()))
            kill(SH_PID, SIGTTIN);             

    act_chld.sa_handler = sigchld_handler;
    act_int.sa_handler = sigint_handler;

    sigaction(SIGCHLD, &act_chld, 0);
    sigaction(SIGINT, &act_int, 0);

    setpgid(SH_PID, SH_PID);
    SH_PGID = getpgrp();

    tcsetpgrp(STDIN_FILENO, SH_PGID);
    tcgetattr(STDIN_FILENO, &SH_TMODES);
    /*tcgetattr()*/
    /*printf("init ok\n");*/

    setenv("OLDPWD", "", 1);
    
}

void sh_loop(void) {
    char buffer[1024];
    char *tokens[64];
    int inStatus;

    do {
        if (!no_reprint) printPrompt();
        no_reprint = 0;
        memset(buffer, '\0', sizeof(buffer));
        inStatus = fgets(buffer, sizeof(buffer), stdin) ? 1 : 0;
        /*printf("\nbuf: %s, s: %d, errno :%d, feof: %d\n",*/
                /*buffer, inStatus, errno, feof(stdin));*/
        // reach eof or C-D
        if (feof(stdin)) break;
        // fgets interrupted
        if (inStatus == 0 && errno == EINTR) {
            continue;
        }
        // fgets correct
        if (inStatus) {
            toArgv(buffer, tokens);
            lunchCmd(tokens);
        }
        /*printf("%d", inStatus);*/
    } while (1);
}

void printPrompt() {
	char hostname[256] = "";
    gethostname(hostname, sizeof(hostname));
    char cwd[1024] = "";
    getcwd(cwd, sizeof(cwd));
	printf("%s@%s %s > ", getenv("LOGNAME"), hostname, cwd);
}

void toArgv(char *buffer, char **argv) {
    *argv++ = strtok(buffer, TOK_DELIM);
    while ((*argv++ = strtok(NULL, TOK_DELIM)));
}

int lunchCmd(char *tokens[]) {
    char *argv[64];
    int lengthOfTokens = 0;
    char cwd[1024] = "";
    getcwd(cwd, sizeof(cwd));
    int background = 0;

    /* find token length */
    for (; tokens[lengthOfTokens] != NULL; lengthOfTokens++) ;

    /*printf("%d\n", lengthOfTokens);*/

    /* no input */
    if (lengthOfTokens == 0) {
        return 0;
    }

    /* built-in function */
    if (strcmp(tokens[0], "exit") == 0) {
        exit(0);
    }
    else if (strcmp(tokens[0], "pwd") == 0) {
        printf("%s\n", cwd);
        return 1;
    }
    else if (strcmp(tokens[0], "cd") == 0) {
        if (tokens[1] == NULL) {
            chdir(getenv("HOME"));
            setenv("OLDPWD", cwd, 1);
        }
        else if (strcmp("~", tokens[1]) == 0) {
            chdir(getenv("HOME"));
            setenv("OLDPWD", cwd, 1);
        }
        else if (strcmp("-", tokens[1]) == 0) {
            chdir(getenv("OLDPWD"));
            setenv("OLDPWD", cwd, 1);
        }
        else {
            if (chdir(tokens[1]) == -1) {
                printf("cd: no such file or directory: %s\n", tokens[1]);
            } else {
                setenv("OLDPWD", cwd, 1);
            }
        }
        return 1;
    }
    lunchProg(tokens, isBackground(tokens, lengthOfTokens));
    return 1;
}

int isBackground(char *tokens[], int l) {
    if (strcmp("&", tokens[l-1]) == 0) {
        tokens[l-1] = NULL;
        return 1;
    }
    else {
        return 0;
    }
}

void lunchProg(char *tokens[], int isBackground) {
    if ((child_pid = fork()) == -1) {
        printf("ysh: fork: Resource temporarily unavailable");
        return;
    }
    if (child_pid == 0) {                   // child
        signal(SIGINT, SIG_IGN);

        if (execvp(tokens[0], tokens) == -1) {
            fprintf(stderr, "ysh: Command not found: %s\n", tokens[0]);
            kill(getpid(), SIGTERM);
        }
    }
    else {                                  // parent
        if (isBackground) {
            printf("[%d] %d\n", 1, child_pid);
        }
        else {
            waitpid(child_pid, NULL, 0);
        }
    }
}
