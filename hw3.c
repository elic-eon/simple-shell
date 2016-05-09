#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <termios.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glob.h>

#define TOK_DELIM " \t\r\n\a"

static pid_t SH_PID;
static pid_t SH_PGID;
static struct termios SH_TMODES;
pid_t child_pid;
struct sigaction act_int;
struct sigaction act_quit;
struct sigaction act_chld;
int no_reprint;
pid_t new_pgrp;
int shell_termial;

void sigchld_handler (int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}
void sigint_handler (int sig) {
    if (new_pgrp > 0 && killpg(new_pgrp, SIGTERM) == 0) {
        printf("kill: %d\n", child_pid);
        no_reprint = 1;
    }
    else {
        printf("int \n");
    }
}
void sigquit_handler (int sig) {
    if (kill(child_pid, SIGQUIT) == 0) {
        printf("kill: %d\n", child_pid);
        no_reprint = 1;
    }
    else {
        printf("quit \n");
    }
}

void initialize(void);
void sh_loop(void);
void printPrompt(void);
void toArgv(char *buffer, char **argv);
int lunchCmd(char *tokens[]);
int isBackground(char *tokens[], int l);
void lunchProg(char *tokens[], int l, int isBackground);
int spawn_proc(int in, int out, char **argv, int isBackground);

int main() {
    initialize();
    sh_loop();
    return 0;
}

void initialize(void) {
    shell_termial = STDIN_FILENO;
    child_pid = -10;
    no_reprint = 0;
    SH_PID = getpid();

    while (tcgetpgrp(shell_termial) != (SH_PGID = getpgrp()))
            kill(SH_PID, SIGTTIN);             

    act_chld.sa_handler = sigchld_handler;
    act_int.sa_handler = sigint_handler;
    act_quit.sa_handler = sigquit_handler;

    sigaction(SIGCHLD, &act_chld, 0);
    sigaction(SIGINT, &act_int, 0);
    sigaction(SIGQUIT, &act_quit, 0);
    signal(SIGTTOU, SIG_IGN);

    setpgid(SH_PID, SH_PID);
    SH_PGID = getpgrp();

    tcsetpgrp(shell_termial, SH_PGID);
    tcgetattr(shell_termial, &SH_TMODES);

    setenv("OLDPWD", "", 1);
    
}

void sh_loop(void) {
    char buffer[1024];
    char *tokens[64];
    int inStatus;

    do {
        if (!no_reprint) {
            printPrompt();
        }
        no_reprint = 0;
        memset(buffer, '\0', sizeof(buffer));
        inStatus = fgets(buffer, sizeof(buffer), stdin) ? 1 : 0;
        // reach eof or C-D
        if (feof(stdin)) break;
        // fgets interrupted
        if (inStatus == 0 && errno == EINTR) {
            continue;
        }
        // fgets correct
        if (inStatus) {
            toArgv(buffer, tokens);
            new_pgrp = 0;
            lunchCmd(tokens);
        }
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
    int lengthOfTokens = 0;
    char cwd[1024] = "";
    getcwd(cwd, sizeof(cwd));

    /* find token length */
    for (; tokens[lengthOfTokens] != NULL; lengthOfTokens++) ;

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
    int b = isBackground(tokens, lengthOfTokens);
    if (b) lengthOfTokens--;
    lunchProg(tokens, lengthOfTokens,b);
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

int spawn_proc(int in, int out, char **argv, int isBackground) {
    pid_t pid;
    glob_t globbuf;
    char *name = argv[0];
    int i;
    glob(name, GLOB_NOCHECK, NULL, &globbuf);
    for (name = argv[1], i = 1; name != NULL; i++, name = argv[i]) {
        glob(name, GLOB_NOCHECK | GLOB_APPEND | GLOB_TILDE, NULL, &globbuf);
    }

    if ((pid = fork ()) == 0) { // child
        pid = getpid();
        if (new_pgrp == 0) {
            new_pgrp = pid;
        }
        setpgid(pid, new_pgrp);
        if (!isBackground) {
            tcsetpgrp(shell_termial, new_pgrp);
        }
        if (in != STDIN_FILENO) {
            dup2 (in, 0);
            close (in);
        }
        if (out != STDOUT_FILENO) { 
            dup2 (out, 1);
            close (out);
        }
        return execvp(globbuf.gl_pathv[0], (char * const *)globbuf.gl_pathv);
    }
    else if (pid > 0) {
        if (isBackground) {
            printf("[%d] %d", 1, pid);
        }
        if (new_pgrp == 0) {
            new_pgrp = pid;
        }
        setpgid(pid, new_pgrp);
    }

    return pid;
}

void lunchProg(char *tokens[], int l, int isBackground) {
    int fd = STDIN_FILENO;
    int fds[2];
    int in, out;
    char *outputFile = NULL;
    char *inputFile = NULL;
    char *arrayOfArgs[64][64];
    int numOfPIPE = 0;
    memset(arrayOfArgs, 0, sizeof(arrayOfArgs));
    for (int i = 0, j = 0; i < l; i++) {
        if (strcmp("<", tokens[i]) == 0) {
            inputFile = tokens[++i];
        }
        else if (strcmp(">", tokens[i]) == 0) {
            outputFile = tokens[++i];
        }
        else if (strcmp("|", tokens[i]) == 0) {
            arrayOfArgs[numOfPIPE][j] = NULL;
            numOfPIPE++;
            j = 0;
        }
        else {
            arrayOfArgs[numOfPIPE][j++] = tokens[i];
        }
    }

    in = dup(STDIN_FILENO);
    out = dup(STDOUT_FILENO);
    if (inputFile != NULL) {
        fd = open(inputFile, O_RDONLY, 0600);
    }
    for (int i = 0; i < numOfPIPE; i++) {
        pipe(fds);
        child_pid = spawn_proc(fd, fds[1], arrayOfArgs[i], isBackground);
        close (fds[1]);
        fd = fds[0];
    }

    if (fd != STDIN_FILENO) {
        dup2 (fd, STDIN_FILENO);
        close(fd);
    }
    if (outputFile != NULL) {
        fd = open(outputFile, O_CREAT | O_TRUNC | O_WRONLY, 0600);
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    child_pid = spawn_proc(STDIN_FILENO, STDOUT_FILENO,
            arrayOfArgs[numOfPIPE], isBackground);
    
    dup2(in, STDIN_FILENO);
    close(in);
    dup2(out, STDOUT_FILENO);
    close(out);

    if (!isBackground) {
        pid_t pid_tmp;
        tcsetpgrp(shell_termial, new_pgrp);
        do {
            pid_tmp = waitpid(WAIT_ANY, NULL, WUNTRACED);
        } while (pid_tmp > 0);
        tcsetpgrp(shell_termial, SH_PGID);
        tcsetattr(shell_termial, TCSADRAIN, &SH_TMODES);
    }
    else {
        tcsetpgrp(shell_termial, SH_PGID);
        tcsetattr(shell_termial, TCSADRAIN, &SH_TMODES);
    }
}
