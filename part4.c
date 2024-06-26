#include <stdio.h>  
#include <stdlib.h>  
#include <unistd.h>  
#include "copytree.h"  

void show_usage(const char *program_name)
{
    // Function to display usage information
    fprintf(stderr, "Usage: %s [-l] [-p] <source_directory> <destination_directory>\n", program_name);
    fprintf(stderr, "  -l: Copy symbolic links as links\n");
    fprintf(stderr, "  -p: Copy file permissions\n");
}

int main(int argc, char *argv[])
{
    int option;
    int do_copy_symlinks = 0;
    int do_copy_permissions = 0;

    // Parse command-line options
    while ((option = getopt(argc, argv, "lp")) != -1)
    {
        switch (option)
        {
            case 'l':
                do_copy_symlinks = 1;  // Set flag to copy symbolic links
                break;
            case 'p':
                do_copy_permissions = 1;  // Set flag to copy file permissions
                break;
            default:
                show_usage(argv[0]);  // Display usage information for incorrect options
                return EXIT_FAILURE;  // Exit with failure status
        }
    }

    // Check if correct number of arguments are provided
    if (optind + 2 != argc)
    {
        show_usage(argv[0]);  // Display usage information if arguments are incorrect
        return EXIT_FAILURE;  // Exit with failure status
    }

    // Extract source and destination directories
    const char *source_dir = argv[optind];
    const char *destination_dir = argv[optind + 1];

    // Call function to copy directory
    copy_directory(source_dir, destination_dir, do_copy_symlinks, do_copy_permissions);

    return 0;  // Exit with success status
}
