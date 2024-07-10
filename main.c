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
        printf("Error allocating BoundedBuffer\n");
        perror("Reason");
        exit(EXIT_FAILURE);
    }
    
    bb->buffer = (char **)malloc(size * sizeof(char *));
    if (bb->buffer == NULL) {
        printf("Error allocating buffer in BoundedBuffer\n");
        perror("Reason");
        exit(EXIT_FAILURE);
    }
    
    for (int i = 0; i < size; i++) {
        bb->buffer[i] = (char *)malloc(MAX_STRING_LENGTH * sizeof(char));
        if (bb->buffer[i] == NULL) {
            printf("Error allocating buffer element in BoundedBuffer\n");
            perror("Reason");
            exit(EXIT_FAILURE);
        }
    }
    
    bb->size = size;
    bb->in = 0;
    bb->out = 0;
    
    if (sem_init(&bb->full, 0, 0) != 0) {
        printf("Error initializing full semaphore in BoundedBuffer\n");
        perror("Reason");
        exit(EXIT_FAILURE);
    }
    
    if (sem_init(&bb->empty, 0, size) != 0) {
        printf("Error initializing empty semaphore in BoundedBuffer\n");
        perror("Reason");
        exit(EXIT_FAILURE);
    }
    
    if (pthread_mutex_init(&bb->mutex, NULL) != 0) {
        printf("Error initializing mutex in BoundedBuffer\n");
        perror("Reason");
        exit(EXIT_FAILURE);
    }
    
    return bb;
}

void insert_bounded_buffer(BoundedBuffer *bb, const char *str) {
    // Check if the mutex has been initialized
    if (pthread_mutex_lock(&bb->mutex) != 0) {
        printf("Error locking mutex\n");
        perror("Reason");
        exit(EXIT_FAILURE);
    }

    strncpy(bb->buffer[bb->in], str, MAX_STRING_LENGTH);
    bb->in = (bb->in + 1) % bb->size;

    // Unlock the mutex only if it was successfully locked
    if (pthread_mutex_unlock(&bb->mutex) != 0) {
        printf("Error unlocking mutex\n");
        perror("Reason");
        exit(EXIT_FAILURE);
    }

    if (sem_wait(&bb->empty) != 0) {
        printf("Error waiting on empty semaphore\n");
        perror("Reason");
        exit(EXIT_FAILURE);
    }

    if (sem_post(&bb->full) != 0) {
        printf("Error posting to full semaphore\n");
        perror("Reason");
        exit(EXIT_FAILURE);
    }
}


char *remove_bounded_buffer(BoundedBuffer *bb) {
    // Check if the mutex has been initialized
    if (pthread_mutex_lock(&bb->mutex) != 0) {
        printf("Error locking mutex\n");
        perror("Reason");
        exit(EXIT_FAILURE);
    }

    char *str = strdup(bb->buffer[bb->out]); // Make a copy of the string to return
    bb->out = (bb->out + 1) % bb->size;

    // Unlock the mutex only if it was successfully locked
    if (pthread_mutex_unlock(&bb->mutex) != 0) {
        printf("Error unlocking mutex\n");
        perror("Reason");
        exit(EXIT_FAILURE);
    }

    if (sem_wait(&bb->full) != 0) {
        printf("Error waiting on full semaphore\n");
        perror("Reason");
        exit(EXIT_FAILURE);
    }

    if (sem_post(&bb->empty) != 0) {
        printf("Error posting to empty semaphore\n");
        perror("Reason");
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
        printf("Error destroying full semaphore\n");
        perror("Reason");
        exit(EXIT_FAILURE);
    }
    
    if (sem_destroy(&bb->empty) != 0) {
        printf("Error destroying empty semaphore\n");
        perror("Reason");
        exit(EXIT_FAILURE);
    }
    
    if (pthread_mutex_destroy(&bb->mutex) != 0) {
        printf("Error destroying mutex\n");
        perror("Reason");
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
            printf("Screen manager displaying message: %s\n", message);
            free(message); // Free the allocated message memory
        }
    }
    return NULL;
}

int main() {
    // Initialize your program
    srand(time(NULL));
    BoundedBuffer producer_queues[3];
    for (int i = 0; i < NUM_TYPES; i++) {
        producer_queues[i] = *create_bounded_buffer(10);
    }
    
    BoundedBuffer dispatcher_queues[3];
    for (int i = 0; i < NUM_TYPES; i++) {
        dispatcher_queues[i] = *create_bounded_buffer(10);
    }
    
    BoundedBuffer shared_queue = *create_bounded_buffer(10);
    
    pthread_t producers[NUM_TYPES];
    ProducerArgs p_args[NUM_TYPES];
    for (int i = 0; i < NUM_TYPES; i++) {
        p_args[i].id = i + 1;
        p_args[i].num_products = 5; // Number of products each producer will produce
        p_args[i].queue = &producer_queues[i];
        pthread_create(&producers[i], NULL, producer_thread, (void *)&p_args[i]);
    }
    
    pthread_t dispatcher;
    DispatcherArgs d_args;
    d_args.producer_queues = producer_queues;
    d_args.dispatcher_queues = dispatcher_queues;
    d_args.num_producers = NUM_TYPES;
    pthread_create(&dispatcher, NULL, dispatcher_thread, (void *)&d_args);
    
    pthread_t co_editors[NUM_TYPES];
    CoEditorArgs ce_args[NUM_TYPES];
    const char *types[NUM_TYPES] = {"SPORTS", "NEWS", "WEATHER"};
    for (int i = 0; i < NUM_TYPES; i++) {
        ce_args[i].dispatcher_queue = &dispatcher_queues[i];
        ce_args[i].shared_queue = &shared_queue;
        ce_args[i].type = types[i];
        pthread_create(&co_editors[i], NULL, co_editor_thread, (void *)&ce_args[i]);
    }
    
    pthread_t screen_manager;
    ScreenManagerArgs sm_args;
    sm_args.shared_queue = &shared_queue;
    pthread_create(&screen_manager, NULL, screen_manager_thread, (void *)&sm_args);
    
    // Join threads
    for (int i = 0; i < NUM_TYPES; i++) {
        pthread_join(producers[i], NULL);
    }
    pthread_join(dispatcher, NULL);
    for (int i = 0; i < NUM_TYPES; i++) {
        pthread_join(co_editors[i], NULL);
    }
    pthread_join(screen_manager, NULL);
    
    // Clean up resources
    for (int i = 0; i < NUM_TYPES; i++) {
        destroy_bounded_buffer(&producer_queues[i]);
    }
    for (int i = 0; i < NUM_TYPES; i++) {
        destroy_bounded_buffer(&dispatcher_queues[i]);
    }
    destroy_bounded_buffer(&shared_queue);
    
    return 0;
}
