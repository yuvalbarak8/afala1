// copytree.c
#include "copytree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>

void copy_file(const char *src, const char *dest, int copy_symlinks, int copy_permissions) {
    struct stat stat_buf;
    if (lstat(src, &stat_buf) == -1) {
        perror("lstat failed");
        return;
    }

    if (S_ISLNK(stat_buf.st_mode) && copy_symlinks) {
        // Copy symbolic link itself, not the target it points to
        char link_target[PATH_MAX];
        ssize_t len = readlink(src, link_target, sizeof(link_target) - 1);
        if (len == -1) {
            perror("readlink failed");
            return;
        }
        link_target[len] = '\0';

        if (symlink(link_target, dest) == -1) {
            perror("symlink failed");
            return;
        }

        printf("Created symlink: %s -> %s\n", dest, link_target);
    } else {
        // Copy regular files or the target of symbolic links when copy_symlinks is not set
        int src_fd = open(src, O_RDONLY);
        if (src_fd == -1) {
            perror("open failed");
            return;
        }

        int dest_fd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, stat_buf.st_mode & 0777);
        if (dest_fd == -1) {
            perror("open failed");
            close(src_fd);
            return;
        }

        char buf[8192];
        ssize_t bytes;
        while ((bytes = read(src_fd, buf, sizeof(buf))) > 0) {
            if (write(dest_fd, buf, bytes) != bytes) {
                perror("write failed");
                close(src_fd);
                close(dest_fd);
                return;
            }
        }

        if (bytes == -1) {
            perror("read failed");
        }

        close(src_fd);
        close(dest_fd);

        if (copy_permissions) {
            if (chmod(dest, stat_buf.st_mode) == -1) {
                perror("chmod failed");
            }
        }

        printf("Copied file: %s -> %s\n", src, dest);
    }
}

void create_directory(const char *path) {
    char tmp[PATH_MAX];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = '\0';

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, S_IRWXU) == -1 && errno != EEXIST) {
                perror("mkdir failed");
                return;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, S_IRWXU) == -1 && errno != EEXIST) {
        perror("mkdir failed");
    }

    printf("Created directory: %s\n", path);
}

void copy_directory(const char *src, const char *dest, int copy_symlinks, int copy_permissions) {
    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(src))) {
        perror("opendir failed");
        return;
    }

    create_directory(dest);

    while ((entry = readdir(dir)) != NULL) {
        char src_path[PATH_MAX];
        char dest_path[PATH_MAX];
        snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
        snprintf(dest_path, sizeof(dest_path), "%s/%s", dest, entry->d_name);

        struct stat stat_buf;
        if (lstat(src_path, &stat_buf) == -1) {
            perror("lstat failed");
            continue;
        }

        if (S_ISDIR(stat_buf.st_mode)) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            copy_directory(src_path, dest_path, copy_symlinks, copy_permissions);
        } else {
            copy_file(src_path, dest_path, copy_symlinks, copy_permissions);
        }
    }
    closedir(dir);
}
