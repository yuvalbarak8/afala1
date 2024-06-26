#include "buffered_open.h"
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

// Helper function to flush the write buffer
static int flush_write_buffer(buffered_file_t *bf) {
    if (bf->write_buffer_pos > 0) {
        ssize_t written = write(bf->fd, bf->write_buffer, bf->write_buffer_pos);
        if (written == -1) {
            return -1;
        }
        bf->write_buffer_pos = 0;
    }
    return 0;
}

// Buffered open function
buffered_file_t *buffered_open(const char *pathname, int flags, ...) {
    buffered_file_t *bf = (buffered_file_t *)malloc(sizeof(buffered_file_t));
    if (!bf) {
        return NULL;
    }

    va_list args;
    va_start(args, flags);
    mode_t mode = 0;
    if (flags & O_CREAT) {
        mode = va_arg(args, int);
    }
    va_end(args);

    bf->preappend = (flags & O_PREAPPEND) ? 1 : 0;
    flags &= ~O_PREAPPEND;

    bf->fd = open(pathname, flags, mode);
    if (bf->fd == -1) {
        free(bf);
        return NULL;
    }

    bf->read_buffer = (char *)malloc(BUFFER_SIZE);
    bf->write_buffer = (char *)malloc(BUFFER_SIZE);
    if (!bf->read_buffer || !bf->write_buffer) {
        close(bf->fd);
        free(bf->read_buffer);
        free(bf->write_buffer);
        free(bf);
        return NULL;
    }

    bf->read_buffer_size = 0;
    bf->write_buffer_size = 0;
    bf->read_buffer_pos = 0;
    bf->write_buffer_pos = 0;
    bf->flags = flags;

    return bf;
}

// Buffered write function
ssize_t buffered_write(buffered_file_t *bf, const void *buf, size_t count) {
    if (bf->preappend) {
        // Read the existing content of the file into a temporary buffer
        off_t current_offset = lseek(bf->fd, 0, SEEK_END);
        char *temp_buffer = (char *)malloc(current_offset);
        if (!temp_buffer) {
            errno = ENOMEM;
            return -1;
        }

        lseek(bf->fd, 0, SEEK_SET);
        if (read(bf->fd, temp_buffer, current_offset) == -1) {
            free(temp_buffer);
            return -1;
        }

        // Write the new data to the beginning
        lseek(bf->fd, 0, SEEK_SET);
        if (write(bf->fd, buf, count) == -1) {
            free(temp_buffer);
            return -1;
        }

        // Append the existing content back to the file
        if (write(bf->fd, temp_buffer, current_offset) == -1) {
            free(temp_buffer);
            return -1;
        }

        free(temp_buffer);
        bf->preappend = 0; // Clear the preappend flag after the first use
        return count;
    }

    const char *buffer = (const char *)buf;
    size_t remaining = count;

    while (remaining > 0) {
        size_t space = BUFFER_SIZE - bf->write_buffer_pos;
        size_t to_write = (remaining < space) ? remaining : space;

        memcpy(bf->write_buffer + bf->write_buffer_pos, buffer, to_write);
        bf->write_buffer_pos += to_write;
        buffer += to_write;
        remaining -= to_write;

        if (bf->write_buffer_pos == BUFFER_SIZE) {
            if (flush_write_buffer(bf) == -1) {
                return -1;
            }
        }
    }

    return count;
}

// Buffered read function
ssize_t buffered_read(buffered_file_t *bf, void *buf, size_t count) {
    char *buffer = (char *)buf;
    size_t remaining = count;

    while (remaining > 0) {
        if (bf->read_buffer_pos == bf->read_buffer_size) {
            bf->read_buffer_size = read(bf->fd, bf->read_buffer, BUFFER_SIZE);
            if (bf->read_buffer_size == -1) {
                return -1;
            }
            if (bf->read_buffer_size == 0) {
                break;
            }
            bf->read_buffer_pos = 0;
        }

        size_t to_read = bf->read_buffer_size - bf->read_buffer_pos;
        if (to_read > remaining) {
            to_read = remaining;
        }

        memcpy(buffer, bf->read_buffer + bf->read_buffer_pos, to_read);
        bf->read_buffer_pos += to_read;
        buffer += to_read;
        remaining -= to_read;
    }

    return count - remaining;
}

// Flush function
int buffered_flush(buffered_file_t *bf) {
    return flush_write_buffer(bf);
}

// Close function
int buffered_close(buffered_file_t *bf) {
    int result = 0;

    if (bf->write_buffer_pos > 0) {
        if (flush_write_buffer(bf) == -1) {
            result = -1;
        }
    }

    if (close(bf->fd) == -1) {
        result = -1;
    }

    free(bf->read_buffer);
    free(bf->write_buffer);
    free(bf);

    return result;
}
