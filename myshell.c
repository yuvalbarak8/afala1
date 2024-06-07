#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_COMMANDS 100
#define MAX_COMMAND_LENGTH 100

char *history[MAX_COMMANDS];
int history_count = 0;

void add_to_history(char *command) {
    if (history_count < MAX_COMMANDS) {
        history[history_count++] = strdup(command);
    } else {
        free(history[0]);
        for (int i = 1; i < MAX_COMMANDS; i++) {
            history[i - 1] = history[i];
        }
        history[MAX_COMMANDS - 1] = strdup(command);
    }
}

void print_history() {
    for (int i = 0; i < history_count; i++) {
        printf("%d %s\n", i + 1, history[i]);
    }
}

void execute_command(char *command, char *paths[], int path_count) {
    char *args[MAX_COMMAND_LENGTH / 2 + 1]; // פקודה + מקסימום חצי אורך הארגומנטים
    char *token = strtok(command, " ");
    int arg_count = 0;

    while (token != NULL) {
        args[arg_count++] = token;
        token = strtok(NULL, " ");
    }
    args[arg_count] = NULL;

    if (arg_count == 0) {
        return;
    }

    if (strcmp(args[0], "cd") == 0) {
        if (arg_count < 2) {
            fprintf(stderr, "cd requires an argument\n");
        } else if (chdir(args[1]) != 0) {
            perror("cd failed");
        }
        return;
    }

    if (strcmp(args[0], "pwd") == 0) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s\n", cwd);
        } else {
            perror("pwd failed");
        }
        return;
    }

    if (strcmp(args[0], "history") == 0) {
        print_history();
        return;
    }

    if (strcmp(args[0], "exit") == 0) {
        exit(0);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return;
    }

    if (pid == 0) {
        for (int i = 0; i < path_count; i++) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", paths[i], args[0]);
            execv(path, args);
        }
        execvp(args[0], args);
        perror("exec failed");
        exit(1);
    } else {
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("wait failed");
        }
    }
}

int main(int argc, char *argv[]) {
    char *paths[argc];
    int path_count = argc - 1;

    for (int i = 0; i < path_count; i++) {
        paths[i] = argv[i + 1];
    }

    while (1) {
        printf("$ ");
        fflush(stdout);

        char command[MAX_COMMAND_LENGTH];
        if (fgets(command, sizeof(command), stdin) == NULL) {
            perror("fgets failed");
            continue;
        }

        // הסרת תו '\n' בסוף הפקודה
        size_t length = strlen(command);
        if (command[length - 1] == '\n') {
            command[length - 1] = '\0';
        }

        add_to_history(command);
        execute_command(command, paths, path_count);
    }

    return 0;
}
