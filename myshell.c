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
            closedir(dir);
            return 1;
        }
    }
    closedir(dir);
    return 0;
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
        strncpy(history[command_counter], command, sizeof(history[command_counter]));
        history[command_counter][sizeof(history[command_counter]) - 1] = '\0'; // Ensure null termination
        command_counter++;

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
            // Search for the file named `command` in the directories specified in PATH and additional paths
char result_path[PATH_MAX];
int found = 0;

// If not found, search in the directories specified in PATH
const char *path_env = getenv("PATH");
if (path_env != NULL) {
    char *path = strdup(path_env);
    char *token = strtok(path, ":");
    while (token != NULL && !found) {
        found = search_file(token, command, result_path);
        token = strtok(NULL, ":");
    }
    free(path);
}

// If not found in PATH, search in the paths provided as command-line arguments
for (int i = 1; i < argc && !found; i++) {
    found = search_file(argv[i], command, result_path);
}

if (found) {

    // Execute the found file
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
    } else if (pid == 0) {
        // This is the child process
        execl(result_path, result_path, NULL);
        // If execl returns, it means an error occurred
        perror("execl failed");
        exit(EXIT_FAILURE); // Terminate the child process
    } else {
        // This is the parent process
        // Wait for the child process to finish
        int status;
        waitpid(pid, &status, 0);
    }
} else {
    // Print error message if file is not found
    fprintf(stderr, "exec failed: No such file or directory\n");
}

        }
    }
    return 0;
}
