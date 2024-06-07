#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define MAX_HISTORY 100
#define MAX_COMMAND_LENGTH 100
#define MAX_PATHS 10

char PATH[256] = ".";  // Start with the current directory
char history[MAX_HISTORY][MAX_COMMAND_LENGTH];
char *custom_paths[MAX_PATHS];
int path_count = 0;

void show_history() {
    for(int i = 0; i < MAX_HISTORY; i++) {
        if(strcmp(history[i], "") == 0)
            break;
        printf("%s\n", history[i]);
    }
}

void ls() {
    struct dirent *de;
    struct stat st;
    char fullpath[1024];
    DIR *dr = opendir(PATH);

    if (dr == NULL) {
        perror("opendir");
        return;
    }

    while ((de = readdir(dr)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }

        snprintf(fullpath, sizeof(fullpath), "%s/%s", PATH, de->d_name);

        if (stat(fullpath, &st) == 0) {
            if (S_ISDIR(st.st_mode) || S_ISREG(st.st_mode)) {
                printf("%s ", de->d_name);
            }
        }
    }
    printf("\n");

    closedir(dr);
}

void cd(char *dir_name) {
    if (chdir(dir_name) == -1) {
        perror("chdir");
    }
}

void pwd() {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("getcwd");
    }
}

void execute_command(char *cmd, char **args) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Child process
        for (int i = 0; i < path_count; i++) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", custom_paths[i], cmd);
            execv(path, args);
        }
        // If execv fails, try using system's PATH
        execvp(cmd, args);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
        }
    }
}

int main(int argc, char *argv[]) {
    // Initialize custom paths
    for (int i = 1; i < argc && i < MAX_PATHS; i++) {
        custom_paths[path_count++] = argv[i];
    }

    char command[MAX_COMMAND_LENGTH];
    int command_counter = 0;

    while (1) {
        printf("$ ");
        fflush(stdout);

        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }

        command[strcspn(command, "\n")] = 0;

        if (command_counter < MAX_HISTORY) {
            strcpy(history[command_counter], command);
            command_counter++;
        }

        char *args[MAX_COMMAND_LENGTH / 2 + 1];
        int i = 0;
        char *token = strtok(command, " ");
        while (token != NULL) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;

        if (args[0] == NULL) {
            continue;
        }

        if (strcmp(args[0], "ls") == 0) {
            ls();
        } else if (strcmp(args[0], "history") == 0) {
            show_history();
        } else if (strcmp(args[0], "cd") == 0) {
            if (args[1] == NULL) {
                printf("cd: missing operand\n");
            } else {
                cd(args[1]);
            }
        } else if (strcmp(args[0], "pwd") == 0) {
            pwd();
        } else if (strcmp(args[0], "exit") == 0) {
            break;
        } else {
            execute_command(args[0], args);
        }
    }

    return 0;
}
