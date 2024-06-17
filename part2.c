#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>


void write_message(const char *message, int count) {
    for (int i = 0; i < count; i++) {
        printf("%s\n", message);
        usleep((rand() % 100) * 1000); // Random delay between 0 and 99 milliseconds
    }
}

// Function to set the lockfile
int set_lock() {
    int fd;
    while ((fd = open("lockfile.lock", O_CREAT|O_EXCL|O_WRONLY, 0666)) == -1) {
        if (errno != EEXIST) {
            perror("Set lock error");
            return -1;
        }
        usleep(5000); 
    }
    return fd;
}

// Function to release the lock file
void free_lock(int fd) {
    close(fd);
    if (unlink("lockfile.lock") == -1) {
        perror("Free lock error");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    // Check if the number of arguments is correct
    if (argc < 6) {
        fprintf(stderr, "Usage: %s <message1> <message2> ... <count>\n", argv[0]);
        return 1;
    }
    // Calculation of the amount of processes and prints
    int print_number = atoi(argv[argc - 1]);
    int fork_number = argc - 2;
    // Make the forks
    for (int i = 0; i < fork_number; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("Failed to fork");
            return 1;
        } 
        else if (pid == 0) {
           // Set the lock file
           int fd = set_lock();
           // Write the message
            write_message(argv[i + 1], print_number);
            // Free the lockfile
            free_lock(fd);
            exit(EXIT_SUCCESS);
        }
    }

    // Parent waiting for all children
    for (int i = 0; i < fork_number; i++) {
        int status;
        if (wait(&status) == -1) {
            perror("Failed of wait");
            return 1;
        }
    }

    return 0;
}
