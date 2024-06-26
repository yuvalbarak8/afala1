#ifndef BUFFERED_OPEN_H
#define BUFFERED_OPEN_H

#include <fcntl.h>
#include <unistd.h>

// Define a new flag that doesn't collide with existing flags
#define O_PREAPPEND 0x40000000

// Define the standard buffer size for read and write operations
#define BUFFER_SIZE 4096

// Structure to hold the buffer and original flags
typedef struct {
    int fd;                     // File descriptor for the opened file
    char *read_buffer;          // Buffer for reading operations
    char *write_buffer;         // Buffer for writing operations
    size_t read_buffer_size;    // Size of the read buffer
    size_t write_buffer_size;   // Size of the write buffer
    size_t read_buffer_pos;     // Current position in the read buffer
    size_t write_buffer_pos;    // Current position in the write buffer
    int flags;                  // File flags
    int preappend;              // Flag to indicate if O_PREAPPEND was used
} buffered_file_t;

// Function prototypes
buffered_file_t *buffered_open(const char *pathname, int flags, ...);
ssize_t buffered_write(buffered_file_t *bf, const void *buf, size_t count);
ssize_t buffered_read(buffered_file_t *bf, void *buf, size_t count);
int buffered_flush(buffered_file_t *bf);
int buffered_close(buffered_file_t *bf);

#endif // BUFFERED_OPEN_H
