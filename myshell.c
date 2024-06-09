#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>

char history[100][100];
char cwd[PATH_MAX];

void show_history() {
    for (int i = 0; i < 100; i++) {
        if (strcmp(history[i], "") == 0)
            break;
        printf("%s\n", history[i]);
    }
}

void cd(const char *path) {
    if (chdir(path) != 0) {
        perror("chdir failed");
    }
}

void pwd() {
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("getcwd() error");
    }
}

// Function to search for a file in a given directory
int search_file(const char *dir_path, const char *filename, char *result_path) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        perror("opendir failed");
        return 0;
    }
    struct dirent *entry;
    struct stat file_info;
    int found = 0;
    while ((entry = readdir(dir)) != NULL) {
        char full_path[PATH_MAX];
        snprintf(full_path, PATH_MAX, "%s/%s", dir_path, entry->d_name);
        if (stat(full_path, &file_info) != 0) {
            perror("stat failed");
            closedir(dir);
            return 0;
        }
        if (S_ISREG(file_info.st_mode) && strcmp(entry->d_name, filename) == 0) {
            snprintf(result_path, PATH_MAX, "%s/%s", dir_path, filename);
            found = 1;
            break;
        }
    }
    closedir(dir);
    return found;
}

int main(int argc, char *argv[]) {
    char command[100];
    int command_counter = 0;

    while (1) {
        printf("$ ");
        fflush(stdout);
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break; // handle end of input or error
        }

        // Remove trailing newline character
        command[strcspn(command, "\n")] = '\0';

        // Store command in history
        strncpy(history[command_counter], command, sizeof(history[command_counter]) - 1);
        history[command_counter][sizeof(history[command_counter]) - 1] = '\0'; // Ensure null termination
        command_counter++;
        if (command_counter >= 100) {
            command_counter = 0; // Wrap around
        }

        if (strcmp(command, "history") == 0) {
            show_history();
        } else if (strcmp(command, "pwd") == 0) {
            pwd();
        } else if (strcmp(command, "exit") == 0) {
            break;
        } else if (strncmp(command, "cd ", 3) == 0) {
            const char *path = command + 3;
            cd(path);
        } else {
            // Separate command and arguments
            char *token = strtok(command, " ");
            char *args[10]; // Assuming a maximum of 10 arguments
            int arg_count = 0;
            while (token != NULL && arg_count < 10) {
                args[arg_count++] = token;
                token = strtok(NULL, " ");
            }
            args[arg_count] = NULL; // Null-terminate the arguments array

            // Execute the program
            pid_t pid = fork();
            if (pid == -1) {
                perror("fork failed");
            } else if (pid == 0) {
                // This is the child process
                execvp(args[0], args);
                // If execvp returns, it means an error occurred
                perror("execvp failed");
                exit(EXIT_FAILURE); // Terminate the child process
            } else {
                // This is the parent process
                // Wait for the child process to finish
                int status;
                waitpid(pid, &status, 0);
            }
        }
    }
    return 0;
}
