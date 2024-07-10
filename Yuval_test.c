#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define MAX_STRING_LENGTH 1000
#define NUM_TYPES 3
#define MAX_LINE_LENGTH 100
#define NUM_FIELDS 3  // ID, number, queue size
int print_counter = 0;
typedef struct {
    int id;
    int number;
    int queue_size;
} Producer;

typedef struct {
    char **buffer;
    int size;
    int in;
    int out;
    sem_t full;
    sem_t empty;
    pthread_mutex_t mutex;
} BoundedBuffer;

typedef struct {
    int id;
    int num_products;
    BoundedBuffer *queue;
} ProducerArgs;

typedef struct {
    BoundedBuffer *producer_queues;
    BoundedBuffer *dispatcher_queues;
    int num_producers;
} DispatcherArgs;

typedef struct {
    BoundedBuffer *dispatcher_queue;
    BoundedBuffer *shared_queue;
    const char *type;
} CoEditorArgs;

typedef struct {
    BoundedBuffer *shared_queue;
} ScreenManagerArgs;

BoundedBuffer *create_bounded_buffer(int size);
void insert_bounded_buffer(BoundedBuffer *bb, const char *str);
char *remove_bounded_buffer(BoundedBuffer *bb);
void destroy_bounded_buffer(BoundedBuffer *bb);

void *producer_thread(void *args);
void *dispatcher_thread(void *args);
void *co_editor_thread(void *args);
void *screen_manager_thread(void *args);

BoundedBuffer *create_bounded_buffer(int size) {
    BoundedBuffer *bb = (BoundedBuffer *)malloc(sizeof(BoundedBuffer));
    if (bb == NULL) {
        perror("Error allocating BoundedBuffer");
        exit(EXIT_FAILURE);
    }
    
    bb->buffer = (char **)malloc(size * sizeof(char *));
    if (bb->buffer == NULL) {
        perror("Error allocating buffer in BoundedBuffer");
        exit(EXIT_FAILURE);
    }
    
    for (int i = 0; i < size; i++) {
        bb->buffer[i] = (char *)malloc(MAX_STRING_LENGTH * sizeof(char));
        if (bb->buffer[i] == NULL) {
            perror("Error allocating buffer element in BoundedBuffer");
            exit(EXIT_FAILURE);
        }
    }
    
    bb->size = size;
    bb->in = 0;
    bb->out = 0;
    
    if (sem_init(&bb->full, 0, 0) != 0) {
        perror("Error initializing full semaphore in BoundedBuffer");
        exit(EXIT_FAILURE);
    }
    
    if (sem_init(&bb->empty, 0, size) != 0) {
        perror("Error initializing empty semaphore in BoundedBuffer");
        exit(EXIT_FAILURE);
    }
    
    if (pthread_mutex_init(&bb->mutex, NULL) != 0) {
        perror("Error initializing mutex in BoundedBuffer");
        exit(EXIT_FAILURE);
    }
    
    return bb;
}

void insert_bounded_buffer(BoundedBuffer *bb, const char *str) {
    if (sem_wait(&bb->empty) != 0) {
        perror("Error waiting on empty semaphore");
        exit(EXIT_FAILURE);
    }
    
    if (pthread_mutex_lock(&bb->mutex) != 0) {
        perror("Error locking mutex");
        exit(EXIT_FAILURE);
    }
    
    strncpy(bb->buffer[bb->in], str, MAX_STRING_LENGTH);
    bb->in = (bb->in + 1) % bb->size;
    
    if (pthread_mutex_unlock(&bb->mutex) != 0) {
        perror("Error unlocking mutex");
        exit(EXIT_FAILURE);
    }
    
    if (sem_post(&bb->full) != 0) {
        perror("Error posting to full semaphore");
        exit(EXIT_FAILURE);
    }
}

char *remove_bounded_buffer(BoundedBuffer *bb) {
    if (sem_wait(&bb->full) != 0) {
        perror("Error waiting on full semaphore");
        exit(EXIT_FAILURE);
    }
    
    if (pthread_mutex_lock(&bb->mutex) != 0) {
        perror("Error locking mutex");
        exit(EXIT_FAILURE);
    }
    
    char *str = strdup(bb->buffer[bb->out]); // Make a copy of the string to return
    bb->out = (bb->out + 1) % bb->size;
    
    if (pthread_mutex_unlock(&bb->mutex) != 0) {
        perror("Error unlocking mutex");
        exit(EXIT_FAILURE);
    }
    
    if (sem_post(&bb->empty) != 0) {
        perror("Error posting to empty semaphore");
        exit(EXIT_FAILURE);
    }
    
    return str;
}

void destroy_bounded_buffer(BoundedBuffer *bb) {
    for (int i = 0; i < bb->size; i++) {
        free(bb->buffer[i]);
    }
    free(bb->buffer);
    
    if (sem_destroy(&bb->full) != 0) {
        perror("Error destroying full semaphore");
        exit(EXIT_FAILURE);
    }
    
    if (sem_destroy(&bb->empty) != 0) {
        perror("Error destroying empty semaphore");
        exit(EXIT_FAILURE);
    }
    
    if (pthread_mutex_destroy(&bb->mutex) != 0) {
        perror("Error destroying mutex");
        exit(EXIT_FAILURE);
    }
    
    free(bb);
}

void *producer_thread(void *args) {
    ProducerArgs *p_args = (ProducerArgs *)args;
    const char *types[] = {"SPORTS", "NEWS", "WEATHER"};
    for (int i = 0; i < p_args->num_products; i++) {
        int type_idx = rand() % NUM_TYPES;
        char product[MAX_STRING_LENGTH];
        snprintf(product, MAX_STRING_LENGTH, "Producer %d %s %d", p_args->id, types[type_idx], i);
        insert_bounded_buffer(p_args->queue, product);
        printf("Producer %d produced %s\n", p_args->id, product);
    }
    insert_bounded_buffer(p_args->queue, "DONE");
    printf("Producer %d done\n", p_args->id);
    return NULL;
}

void *dispatcher_thread(void *args) {
    DispatcherArgs *d_args = (DispatcherArgs *)args;
    int num_done = 0;
    while (num_done < d_args->num_producers) {
        for (int i = 0; i < d_args->num_producers; i++) {
            if (sem_trywait(&d_args->producer_queues[i].full) == 0) {
                char *message = remove_bounded_buffer(&d_args->producer_queues[i]);
                if (strcmp(message, "DONE") == 0) {
                    num_done++;
                } else {
                    char type[MAX_STRING_LENGTH];
                    sscanf(message, "%*s %*d %s %*d", type);
                    if (strcmp(type, "SPORTS") == 0) {
                        insert_bounded_buffer(&d_args->dispatcher_queues[0], message);
                    } else if (strcmp(type, "NEWS") == 0) {
                        insert_bounded_buffer(&d_args->dispatcher_queues[1], message);
                    } else if (strcmp(type, "WEATHER") == 0) {
                        insert_bounded_buffer(&d_args->dispatcher_queues[2], message);
                    }
                    sem_post(&d_args->producer_queues[i].full);
                    printf("Dispatcher processed message: %s\n", message);
                    free(message); // Free the allocated message memory
                }
            }
        }
    }
    // Signal end of dispatching to co-editors
    for (int i = 0; i < NUM_TYPES; i++) {
        insert_bounded_buffer(&d_args->dispatcher_queues[i], "DONE");
        printf("Dispatcher sent DONE to co-editor %d\n", i);
    }
    return NULL;
}


void *co_editor_thread(void *args) {
    CoEditorArgs *ce_args = (CoEditorArgs *)args;
    while (1) {
        char *message = remove_bounded_buffer(ce_args->dispatcher_queue);
        usleep(100000);  // Simulate editing by sleeping for 0.1 seconds
        insert_bounded_buffer(ce_args->shared_queue, message);
        printf("Co-editor %s edited message: %s\n", ce_args->type, message);
        free(message); // Free the allocated message memory
    }
    return NULL;
}

void *screen_manager_thread(void *args) {
    ScreenManagerArgs *sm_args = (ScreenManagerArgs *)args;
    while (1) {
        char *message = remove_bounded_buffer(sm_args->shared_queue);
        printf("Screen manager displaying message: %s\n", message);
        print_counter++; // Increment print_counter for each message displayed
        printf("%d\n", print_counter);
        if (print_counter >= 90) {
            printf("Done.\n");
            exit(0);
        }
        free(message); // Free the allocated message memory
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    char *filename = argv[1];
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    Producer *producers_array = NULL;
    int max_producers = 10;  // Initial assumption, can be adjusted as needed
    int num_producers = 0;
    int coeditor_queue_size = 0;
    char line[MAX_LINE_LENGTH];
    
    // Allocate memory for producers
    producers_array = (Producer *)malloc(max_producers * sizeof(Producer));
    if (producers_array == NULL) {
        perror("Memory allocation failed");
        return 1;
    }

    while (fgets(line, sizeof(line), file)) {
        // Remove newline character if present
        line[strcspn(line, "\n")] = 0;

        if (strstr(line, "PRODUCER") != NULL) {
            // Resize array if necessary
            if (num_producers >= max_producers) {
                max_producers *= 2;
                producers_array = (Producer *)realloc(producers_array, max_producers * sizeof(Producer));
                if (producers_array == NULL) {
                    perror("Memory reallocation failed");
                    return 1;
                }
            }
            // Read producer information
            sscanf(line, "PRODUCER %d", &producers_array[num_producers].id);
            fgets(line, sizeof(line), file); // Read next line for number
            sscanf(line, "%d", &producers_array[num_producers].number);
            fgets(line, sizeof(line), file); // Read next line for queue size
            sscanf(line, "queue size = %d", &producers_array[num_producers].queue_size);
            num_producers++;
        } else if (strstr(line, "Co-Editor queue size") != NULL) {
            sscanf(line, "Co-Editor queue size = %d", &coeditor_queue_size);
        }
    }

    fclose(file);

    // Print out the data as an example
    printf("Producers:\n");
    for (int i = 0; i < num_producers; i++) {
        printf("Producer %d: ID = %d, Number = %d, Queue Size = %d\n",
               i + 1, producers_array[i].id, producers_array[i].number, producers_array[i].queue_size);
    }
    printf("Co-Editor Queue Size: %d\n", coeditor_queue_size);

    srand(time(NULL));
    
    int BUFFER_SIZE = 100;
    int NUM_PRODUCERS = num_producers;
    
    BoundedBuffer producer_queues[NUM_PRODUCERS];
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        producer_queues[i] = *create_bounded_buffer(BUFFER_SIZE);
    }
    
    BoundedBuffer dispatcher_queues[NUM_TYPES];
    for (int i = 0; i < NUM_TYPES; i++) {
        dispatcher_queues[i] = *create_bounded_buffer(BUFFER_SIZE);
    }
    
    BoundedBuffer shared_queue = *create_bounded_buffer(BUFFER_SIZE * NUM_PRODUCERS);
    
    pthread_t producers[NUM_PRODUCERS];
    ProducerArgs producer_args[NUM_PRODUCERS];
    
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        producer_args[i].id = producers_array[i].id;
        producer_args[i].num_products = producers_array[i].number;
        producer_args[i].queue = &producer_queues[i];
        if (pthread_create(&producers[i], NULL, producer_thread, (void *)&producer_args[i]) != 0) {
            perror("Error creating producer thread");
            exit(EXIT_FAILURE);
        }
    }
    
    DispatcherArgs dispatcher_args = {producer_queues, dispatcher_queues, NUM_PRODUCERS};
    pthread_t dispatcher;
    if (pthread_create(&dispatcher, NULL, dispatcher_thread, (void *)&dispatcher_args) != 0) {
        perror("Error creating dispatcher thread");
        exit(EXIT_FAILURE);
    }
    
    CoEditorArgs co_editor_args[NUM_TYPES];
    pthread_t co_editors[NUM_TYPES];
    
    const char *types[] = {"SPORTS", "NEWS", "WEATHER"};
    for (int i = 0; i < NUM_TYPES; i++) {
        co_editor_args[i].dispatcher_queue = &dispatcher_queues[i];
        co_editor_args[i].shared_queue = &shared_queue;
        co_editor_args[i].type = types[i];
        if (pthread_create(&co_editors[i], NULL, co_editor_thread, (void *)&co_editor_args[i]) != 0) {
            perror("Error creating co-editor thread");
            exit(EXIT_FAILURE);
        }
    }
    
    ScreenManagerArgs screen_manager_args = {&shared_queue};
    pthread_t screen_manager;
    if (pthread_create(&screen_manager, NULL, screen_manager_thread, (void *)&screen_manager_args) != 0) {
        perror("Error creating screen manager thread");
        exit(EXIT_FAILURE);
    }
    
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        if (pthread_join(producers[i], NULL) != 0) {
            perror("Error joining producer thread");
            exit(EXIT_FAILURE);
        }
    }
    
    if (pthread_join(dispatcher, NULL) != 0) {
        perror("Error joining dispatcher thread");
        exit(EXIT_FAILURE);
    }
    
    for (int i = 0; i < NUM_TYPES; i++) {
        if (pthread_join(co_editors[i], NULL) != 0) {
            perror("Error joining co-editor thread");
            exit(EXIT_FAILURE);
        }
    }
    
    if (pthread_join(screen_manager, NULL) != 0) {
        perror("Error joining screen manager thread");
        exit(EXIT_FAILURE);
    }
    
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        destroy_bounded_buffer(&producer_queues[i]);
    }
    
    for (int i = 0; i < NUM_TYPES; i++) {
        destroy_bounded_buffer(&dispatcher_queues[i]);
    }
    
    destroy_bounded_buffer(&shared_queue);
    
    return 0;
}
