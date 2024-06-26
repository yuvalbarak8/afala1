#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "buffered_open.h"

buffered_file_t *buffered_open(const char *pathname, int flags, ...) 
{
    buffered_file_t *bfile = (buffered_file_t *)malloc(sizeof(buffered_file_t));
    if (!bfile) {
        return NULL;
    }

    va_list arg_list;
    va_start(arg_list, flags);
    mode_t file_mode = 0;
    if (flags & O_CREAT) {
        file_mode = va_arg(arg_list, int);
    }
    va_end(arg_list);

    bfile->preappend = (flags & O_PREAPPEND) ? 1 : 0;
    flags &= ~O_PREAPPEND;

    bfile->fd = open(pathname, flags, file_mode);
    if (bfile->fd == -1) {
        free(bfile);
        return NULL;
    }

    bfile->read_buffer = (char *)malloc(BUFFER_SIZE);
    bfile->write_buffer = (char *)malloc(BUFFER_SIZE);
    if (!bfile->read_buffer || !bfile->write_buffer) {
        close(bfile->fd);
        free(bfile->read_buffer);
        free(bfile->write_buffer);
        free(bfile);
        return NULL;
    }

    bfile->read_buffer_size = 0;
    bfile->write_buffer_size = 0;
    bfile->read_buffer_pos = 0;
    bfile->write_buffer_pos = 0;
    bfile->flags = flags;

    return bfile;
}

ssize_t buffered_write(buffered_file_t *bfile, const void *buf, size_t count) 
{
    if (bfile->preappend) {
        off_t end_offset = lseek(bfile->fd, 0, SEEK_END);
        char *temp_buf = (char *)malloc(end_offset);
        if (!temp_buf) {
            errno = ENOMEM;
            return -1;
        }

        lseek(bfile->fd, 0, SEEK_SET);
        if (read(bfile->fd, temp_buf, end_offset) == -1) {
            free(temp_buf);
            return -1;
        }

        lseek(bfile->fd, 0, SEEK_SET);
        if (write(bfile->fd, buf, count) == -1) {
            free(temp_buf);
            return -1;
        }

        if (write(bfile->fd, temp_buf, end_offset) == -1) {
            free(temp_buf);
            return -1;
        }

        free(temp_buf);
        return count;
    }

    const char *input_buf = (const char *)buf;
    size_t bytes_left = count;

    while (bytes_left > 0) {
        if (bfile->write_buffer_pos > 0) {
            ssize_t written_bytes = write(bfile->fd, bfile->write_buffer, bfile->write_buffer_pos);
            if (written_bytes == -1) {
                return -1;
            }
            bfile->write_buffer_pos = 0;
        }

        size_t buffer_space = BUFFER_SIZE - bfile->write_buffer_pos;
        size_t write_bytes = (bytes_left < buffer_space) ? bytes_left : buffer_space;

        memcpy(bfile->write_buffer + bfile->write_buffer_pos, input_buf, write_bytes);
        bfile->write_buffer_pos += write_bytes;
        input_buf += write_bytes;
        bytes_left -= write_bytes;
    }

    return count;
}

ssize_t buffered_read(buffered_file_t *bfile, void *buf, size_t count) 
{
    char *output_buf = (char *)buf;
    size_t bytes_left = count;

    while (bytes_left > 0) {
        if (bfile->read_buffer_pos == bfile->read_buffer_size) {
            bfile->read_buffer_size = read(bfile->fd, bfile->read_buffer, BUFFER_SIZE);
            if (bfile->read_buffer_size == -1) {
                return -1;
            }
            if (bfile->read_buffer_size == 0) {
                break;
            }
            bfile->read_buffer_pos = 0;
        }

        size_t read_bytes = bfile->read_buffer_size - bfile->read_buffer_pos;
        if (read_bytes > bytes_left) {
            read_bytes = bytes_left;
        }

        memcpy(output_buf, bfile->read_buffer + bfile->read_buffer_pos, read_bytes);
        bfile->read_buffer_pos += read_bytes;
        output_buf += read_bytes;
        bytes_left -= read_bytes;
    }

    return count - bytes_left;
}

int buffered_flush(buffered_file_t *bfile) 
{
    if (bfile->write_buffer_pos > 0) {
        ssize_t written_bytes = write(bfile->fd, bfile->write_buffer, bfile->write_buffer_pos);
        if (written_bytes == -1) {
            return -1;
        }
        bfile->write_buffer_pos = 0;
    }

    return 0;
}

int buffered_close(buffered_file_t *bfile) 
{
    int close_result = 0;

    if (bfile->write_buffer_pos > 0) {
        ssize_t written_bytes = write(bfile->fd, bfile->write_buffer, bfile->write_buffer_pos);
        if (written_bytes == -1) {
            close_result = -1;
        }
        bfile->write_buffer_pos = 0;
    }

    if (close(bfile->fd) == -1) {
        close_result = -1;
    }

    free(bfile->read_buffer);
    free(bfile->write_buffer);
    free(bfile);

    return close_result;
}
