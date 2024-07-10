#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define MAX_STRING_LENGTH 100
#define NUM_TYPES 3

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
                }
                sem_post(&d_args->producer_queues[i].full);
                printf("Dispatcher processed message: %s\n", message);
                free(message); // Free the allocated message memory
            }
        }
    }
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
        if (strcmp(message, "DONE") == 0) {
            insert_bounded_buffer(ce_args->shared_queue, "DONE");
            printf("Co-editor %s received DONE\n", ce_args->type);
            free(message); // Free the allocated message memory
            break;
        }
        usleep(100000);  // Simulate editing by sleeping for 0.1 seconds
        insert_bounded_buffer(ce_args->shared_queue, message);
        printf("Co-editor %s edited message: %s\n", ce_args->type, message);
        free(message); // Free the allocated message memory
    }
    return NULL;
}

void *screen_manager_thread(void *args) {
    ScreenManagerArgs *sm_args = (ScreenManagerArgs *)args;
    int num_done = 0;
    while (num_done < NUM_TYPES) {
        char *message = remove_bounded_buffer(sm_args->shared_queue);
        if (strcmp(message, "DONE") == 0) {
            num_done++;
            printf("Screen manager received DONE %d\n", num_done);
            free(message); // Free the allocated message memory
        } else {
            printf("Screen manager displayed: %s\n", message);
            free(message); // Free the allocated message memory
        }
    }
    printf("Screen manager DONE\n");
    return NULL;
}

void read_config(const char *filename, int *num_producers, ProducerArgs **p_args, int *ce_queue_size) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    int temp_num_producers = 0;
    char buffer[256];

    // Count the number of producers in the file.
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        if (strncmp(buffer, "PRODUCER", 8) == 0) {
            temp_num_producers++;
        }
    }

    // Allocate memory for producers
    *num_producers = temp_num_producers;
    *p_args = (ProducerArgs *)malloc(*num_producers * sizeof(ProducerArgs));
    if (*p_args == NULL) {
        perror("Error allocating memory for producers");
        exit(EXIT_FAILURE);
    }

    // Rewind the file to the beginning
    rewind(file);

    // Read producers from the file
    int producer_index = 0;
    while (producer_index < temp_num_producers && fgets(buffer, sizeof(buffer), file) != NULL) {
        if (strncmp(buffer, "PRODUCER", 8) == 0) {
            if (sscanf(buffer, "PRODUCER %d\n", &((*p_args)[producer_index].id)) != 1) {
                fprintf(stderr, "Error: Could not read producer ID\n");
                exit(EXIT_FAILURE);
            }
            if (fscanf(file, "%d\n", &((*p_args)[producer_index].num_products)) != 1) {
                fprintf(stderr, "Error: Could not read number of products\n");
                exit(EXIT_FAILURE);
            }
            int queue_size;
            if (fscanf(file, "queue size = %d\n", &queue_size) != 1) {
                fprintf(stderr, "Error: Could not read queue size\n");
                exit(EXIT_FAILURE);
            }
            (*p_args)[producer_index].queue = create_bounded_buffer(queue_size);
            printf("Configured Producer %d with queue size %d\n", (*p_args)[producer_index].id, queue_size);
            producer_index++;
        }
    }

    // Read Co-Editor queue size from the file
    if (fscanf(file, "Co-Editor queue size = %d\n", ce_queue_size) != 1) {
        fprintf(stderr, "Error: Could not read Co-Editor queue size\n");
        exit(EXIT_FAILURE);
    }
    printf("Configured Co-Editor queue size %d\n", *ce_queue_size);

    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <config file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int num_producers;
    ProducerArgs *p_args;
    int ce_queue_size;

    read_config(argv[1], &num_producers, &p_args, &ce_queue_size);

    // Check for valid queue sizes
    if (ce_queue_size <= 0) {
        fprintf(stderr, "Error: Co-Editor queue size must be greater than 0\n");
        exit(EXIT_FAILURE);
    }

    BoundedBuffer *dispatcher_queues = (BoundedBuffer *)malloc(NUM_TYPES * sizeof(BoundedBuffer));
    if (dispatcher_queues == NULL) {
        perror("Error allocating dispatcher queues");
        exit(EXIT_FAILURE);
    }
    
    for (int i = 0; i < NUM_TYPES; i++) {
        dispatcher_queues[i] = *create_bounded_buffer(ce_queue_size);
        printf("Created dispatcher queue for type %d with size %d\n", i, ce_queue_size);
    }

    BoundedBuffer *shared_queue = create_bounded_buffer(ce_queue_size);
    printf("Created shared queue with size %d\n", ce_queue_size);

    DispatcherArgs d_args = {NULL, dispatcher_queues, num_producers};
    CoEditorArgs ce_args[NUM_TYPES] = {
        {&dispatcher_queues[0], shared_queue, "SPORTS"},
        {&dispatcher_queues[1], shared_queue, "NEWS"},
        {&dispatcher_queues[2], shared_queue, "WEATHER"}
    };
    ScreenManagerArgs sm_args = {shared_queue};

    pthread_t producer_threads[num_producers];
    pthread_t dispatcher_thread_id;
    pthread_t co_editor_threads[NUM_TYPES];
    pthread_t screen_manager_thread_id;

    for (int i = 0; i < num_producers; i++) {
        d_args.producer_queues = p_args[i].queue;
        if (pthread_create(&producer_threads[i], NULL, producer_thread, &p_args[i]) != 0) {
            perror("Error creating producer thread");
            exit(EXIT_FAILURE);
        }
        printf("Started Producer %d\n", p_args[i].id);
    }

    if (pthread_create(&dispatcher_thread_id, NULL, dispatcher_thread, &d_args) != 0) {
        perror("Error creating dispatcher thread");
        exit(EXIT_FAILURE);
    }
    printf("Started Dispatcher\n");

    for (int i = 0; i < NUM_TYPES; i++) {
        if (pthread_create(&co_editor_threads[i], NULL, co_editor_thread, &ce_args[i]) != 0) {
            perror("Error creating co-editor thread");
            exit(EXIT_FAILURE);
        }
        printf("Started Co-Editor %s\n", ce_args[i].type);
    }

    if (pthread_create(&screen_manager_thread_id, NULL, screen_manager_thread, &sm_args) != 0) {
        perror("Error creating screen manager thread");
        exit(EXIT_FAILURE);
    }
    printf("Started Screen Manager\n");

    for (int i = 0; i < num_producers; i++) {
        if (pthread_join(producer_threads[i], NULL) != 0) {
            perror("Error joining producer thread");
            exit(EXIT_FAILURE);
        }
        printf("Joined Producer %d\n", p_args[i].id);
    }

    if (pthread_join(dispatcher_thread_id, NULL) != 0) {
        perror("Error joining dispatcher thread");
        exit(EXIT_FAILURE);
    }
    printf("Joined Dispatcher\n");

    for (int i = 0; i < NUM_TYPES; i++) {
        if (pthread_join(co_editor_threads[i], NULL) != 0) {
            perror("Error joining co-editor thread");
            exit(EXIT_FAILURE);
        }
        printf("Joined Co-Editor %s\n", ce_args[i].type);
    }

    if (pthread_join(screen_manager_thread_id, NULL) != 0) {
        perror("Error joining screen manager thread");
        exit(EXIT_FAILURE);
    }
    printf("Joined Screen Manager\n");

    for (int i = 0; i < num_producers; i++) {
        destroy_bounded_buffer(p_args[i].queue);
    }

    for (int i = 0; i < NUM_TYPES; i++) {
        destroy_bounded_buffer(&dispatcher_queues[i]);
    }

    destroy_bounded_buffer(shared_queue);

    free(p_args);
    free(dispatcher_queues);

    return 0;
}
