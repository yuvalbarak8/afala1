#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_COMMANDS 100
#define MAX_COMMAND_LENGTH 100

// Function prototypes
void add_to_history(char *command);
void print_history();
void execute_command(char *command, char **paths);
int is_builtin_command(char *command);
void handle_builtin_command(char *command);

// Global history array and counter
char *history[MAX_COMMANDS];
int history_count = 0;

int main(int argc, char *argv[]) {
    // Ensure that paths are provided as command-line arguments
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <path1> <path2> ... <pathN>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *paths[argc];
    for (int i = 1; i < argc; i++) {
        paths[i - 1] = argv[i];
    }
    paths[argc - 1] = NULL;

    char command[MAX_COMMAND_LENGTH];

    while (1) {
        printf("$ ");
        fflush(stdout);

        // Read command input
        if (fgets(command, MAX_COMMAND_LENGTH, stdin) == NULL) {
            perror("fgets failed");
            exit(EXIT_FAILURE);
        }

        // Remove the newline character
        command[strcspn(command, "\n")] = '\0';

        // Add command to history
        add_to_history(command);

        // Check if the command is a built-in command
        if (is_builtin_command(command)) {
            handle_builtin_command(command);
        } else {
            execute_command(command, paths);
        }
    }

    return 0;
}

void add_to_history(char *command) {
    if (history_count < MAX_COMMANDS) {
        history[history_count] = strdup(command);
        history_count++;
    }
}

void print_history() {
    for (int i = 0; i < history_count; i++) {
        printf("%d: %s\n", i + 1, history[i]);
    }
}

void execute_command(char *command, char **paths) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        char *args[MAX_COMMAND_LENGTH / 2 + 1];
        char *token = strtok(command, " ");
        int arg_count = 0;

        while (token != NULL) {
            args[arg_count++] = token;
            token = strtok(NULL, " ");
        }
        args[arg_count] = NULL;

        // Try to execute the command in each specified path
        for (int i = 0; paths[i] != NULL; i++) {
            char full_path[MAX_COMMAND_LENGTH];
            snprintf(full_path, sizeof(full_path), "%s/%s", paths[i], args[0]);
            execv(full_path, args);
        }

        // Try to execute the command using the PATH environment variable
        execvp(args[0], args);
        perror("exec failed");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        if (wait(NULL) < 0) {
            perror("wait failed");
            exit(EXIT_FAILURE);
        }
    }
}

int is_builtin_command(char *command) {
    return (strncmp(command, "history", 7) == 0 ||
            strncmp(command, "cd", 2) == 0 ||
            strncmp(command, "pwd", 3) == 0 ||
            strncmp(command, "exit", 4) == 0);
}

void handle_builtin_command(char *command) {
    if (strncmp(command, "history", 7) == 0) {
        print_history();
    } else if (strncmp(command, "cd", 2) == 0) {
        char *path = strtok(command + 3, " ");
        if (path == NULL) {
            fprintf(stderr, "cd: missing argument\n");
        } else if (chdir(path) != 0) {
            perror("cd failed");
        }
    } else if (strncmp(command, "pwd", 3) == 0) {
        char cwd[MAX_COMMAND_LENGTH];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s\n", cwd);
        } else {
            perror("pwd failed");
        }
    } else if (strncmp(command, "exit", 4) == 0) {
        exit(EXIT_SUCCESS);
    }
}
