#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

pid_t child_pid;
/*prevent zombie process*/
static void sigchld_handler (int sig)
{
    while (waitpid(0, NULL, WNOHANG) > 0);
}

/*support ^C*/
void intHandler(int sig) {
    kill(child_pid, SIGKILL);
}

void toArgv(char *buffer, char **argv) {
    *argv++ = strtok(buffer, " \t\n");
    while ((*argv++ = strtok(NULL, " \t\n")));
}

void printPrompt() {
	char *prompt = "myshell> ";
	printf("%s", prompt);
}

int main () {
    char buffer[1024];
    char *argv[64];
    int flag_nowait = 0;

    /*signal mag*/
    struct sigaction act;
    memset (&act, 0, sizeof(act));
    act.sa_handler = sigchld_handler;
    signal(SIGINT, intHandler);
    signal(SIGCHLD, sigchld_handler);
    /*if (sigaction(SIGCHLD, &act, 0) == -1) {*/
        /*perror(0);*/
        /*exit(1);*/
    /*}*/

	printPrompt();
    while (fgets(buffer, sizeof(buffer), stdin)) {
        /*buffer[strlen(buffer)-1] = '\0';*/
        /*parse buffer to argv*/
        toArgv(buffer, argv);

        /*exit*/
        if (strcmp(buffer, "exit") == 0) {
            exit(0);
        }
        else if (strcmp(buffer, "cd") == 0) {
            chdir(argv[1]);
			printPrompt();
            continue;
        }

        /*get argc*/
        int argc = 0;
        for (; argv[argc] != NULL; argc++);
        if (argc == 0) {
			printPrompt();
			continue;
		}

        /*set no_wait*/
        if (strcmp(argv[argc-1], "&") == 0) {
            flag_nowait = 1;
            argv[argc-1] = NULL;
        }
        else {
            flag_nowait = 0;
        }

        if ((child_pid = fork()) < 0) {
            /*fork error*/
            perror(0);
            exit(-1);
        }
        else if (child_pid == 0) {
            /*child*/
            if (execvp(argv[0], argv) < 0) {
                perror(0);
                exit(-1);
            }
        }
        else {
            /*parent*/
            int i;
            for (i = 0; i < 10000000; i++);
            if (!flag_nowait) {
                waitpid(child_pid, NULL, 0);
            }
			printPrompt();
        }
    }
    return 0;
}
