#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    // Check if the number of arguments is correct
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <parent_message> <child1_message> <child2_message> <count>\n", argv[0]);
        return 1;
    }

    // Convert the last argument to an integer (for loop count)
    int count = atoi(argv[4]);

    // Create output.txt
    FILE *output; 
    output = fopen("output.txt", "w"); 
    if (output == NULL) {
        perror("Failed to create the file");
        return 1;
    }

    // Fork process
    pid_t pid = fork();
    
    // Error handling for fork
    if (pid == -1) {
        perror("fork");
        fclose(output); 
        return 1;
    }
    
    // Child process
    if (pid == 0) {
        pid_t sec_pid = fork();
        // Error handling for second fork
        if (sec_pid == -1) {
            perror("fork");
            fclose(output); 
            return 1;
        }
        
        // Second child process
        if (sec_pid == 0) {
            for (int i = 0; i < count; i++) {
                fprintf(output, "%s\n", argv[3]); // Write third argument to file
            }
        } else {
            // First child process
            for (int i = 0; i < count; i++) {
                fprintf(output, "%s\n", argv[2]); // Write second argument to file
            }
        }
    } else {
        // Wait for both children to complete
        if (sleep(5) == -1) {
            perror("Failed of sleep");
            return 1;
        }

        for (int i = 0; i < count; i++) {
            fprintf(output, "%s\n", argv[1]); 
        }
    }

    fclose(output); 

    return 0;
}
