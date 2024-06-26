#include "copytree.h"
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

void copy_file(const char *src, const char *dest, int copy_symlinks, int copy_permissions)
{
    // Creating a struct to store details about the file
    struct stat file_details;
    if (lstat(src, &file_details) == -1)
    {
        perror("lstat failed");
        return;
    }

    // If the file is symlink and the user put the -l flag, we need to make symlink
    if (copy_symlinks && S_ISLNK(file_details.st_mode))
    {
        // Copy symbolic link itself
        char link_target[PATH_MAX];
        ssize_t len = readlink(src, link_target, sizeof(link_target) - 1);
        if (len == -1)
        {
            perror("readlink failed");
            return;
        }
        link_target[len] = '\0';

        // Make the symlink in the dest
        if (symlink(link_target, dest) == -1)
        {
            perror("symlink failed");
            return;
        }
    }
    else
    {
        // Copy the file, not lnk
        int src_fd = open(src, O_RDONLY);
        if (src_fd == -1)
        {
            perror("open failed");
            return;
        }
        
        int dest_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, 0666); 
        if (dest_fd == -1)
        {
            perror("open failed");
            close(src_fd);
            return;
        }

        char buf[8192];
        ssize_t bytes;
        while ((bytes = read(src_fd, buf, sizeof(buf))) > 0)
        {
            if (write(dest_fd, buf, bytes) != bytes)
            {
                perror("write failed");
                close(src_fd);
                close(dest_fd);
                return;
            }
        }

        if (bytes == -1)
        {
            perror("read failed");
        }

        close(src_fd);
        close(dest_fd);

        // Apply permissions if requested
        if (copy_permissions)
        {
            if (chmod(dest, file_details.st_mode) == -1)
            {
                perror("chmod failed");
            }
        }
    }
}

void copy_directory(const char *src, const char *dest, int copy_symlinks, int copy_permissions)
{
    DIR *source_dir;
    struct dirent *entry;
    if (!(source_dir = opendir(src)))
    {
        perror("opendir failed");
        return;
    }

    // Create destination directory if it does not exist
    struct stat st;
    if (stat(dest, &st) == -1)
    {
        if (mkdir(dest, S_IRWXU) == -1 && errno != EEXIST)
        {
            perror("mkdir failed");
            closedir(source_dir);
            return;
        }
    }
    else if (!S_ISDIR(st.st_mode))
    {
        fprintf(stderr, "Destination path exists but is not a directory.\n");
        closedir(source_dir);
        return;
    }

    // Copy all the files and directories
    while ((entry = readdir(source_dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        char src_path[PATH_MAX];
        char dest_path[PATH_MAX];
        snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
        snprintf(dest_path, sizeof(dest_path), "%s/%s", dest, entry->d_name);

        // Creating a struct to store details about the file
        struct stat file_details;
        if (lstat(src_path, &file_details) == -1)
        {
            perror("lstat failed");
            continue;
        }

        // Recursively copy directories
        if (S_ISDIR(file_details.st_mode))
        {
            copy_directory(src_path, dest_path, copy_symlinks, copy_permissions);
        }
        else
        {
            copy_file(src_path, dest_path, copy_symlinks, copy_permissions);
        }
    }

    closedir(source_dir);
}
