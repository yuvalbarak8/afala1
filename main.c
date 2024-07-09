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
    bb->buffer = (char **)malloc(size * sizeof(char *));
    for (int i = 0; i < size; i++) {
        bb->buffer[i] = (char *)malloc(MAX_STRING_LENGTH * sizeof(char));
    }
    bb->size = size;
    bb->in = 0;
    bb->out = 0;
    sem_init(&bb->full, 0, 0);
    sem_init(&bb->empty, 0, size);
    pthread_mutex_init(&bb->mutex, NULL);
    return bb;
}

void insert_bounded_buffer(BoundedBuffer *bb, const char *str) {
    sem_wait(&bb->empty);
    pthread_mutex_lock(&bb->mutex);
    strncpy(bb->buffer[bb->in], str, MAX_STRING_LENGTH);
    bb->in = (bb->in + 1) % bb->size;
    pthread_mutex_unlock(&bb->mutex);
    sem_post(&bb->full);
}

char *remove_bounded_buffer(BoundedBuffer *bb) {
    sem_wait(&bb->full);
    pthread_mutex_lock(&bb->mutex);
    char *str = bb->buffer[bb->out];
    bb->out = (bb->out + 1) % bb->size;
    pthread_mutex_unlock(&bb->mutex);
    sem_post(&bb->empty);
    return str;
}

void destroy_bounded_buffer(BoundedBuffer *bb) {
    for (int i = 0; i < bb->size; i++) {
        free(bb->buffer[i]);
    }
    free(bb->buffer);
    sem_destroy(&bb->full);
    sem_destroy(&bb->empty);
    pthread_mutex_destroy(&bb->mutex);
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
    }
    insert_bounded_buffer(p_args->queue, "DONE");
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
            }
        }
    }
    for (int i = 0; i < NUM_TYPES; i++) {
        insert_bounded_buffer(&d_args->dispatcher_queues[i], "DONE");
    }
    return NULL;
}

void *co_editor_thread(void *args) {
    CoEditorArgs *ce_args = (CoEditorArgs *)args;
    while (1) {
        char *message = remove_bounded_buffer(ce_args->dispatcher_queue);
        if (strcmp(message, "DONE") == 0) {
            insert_bounded_buffer(ce_args->shared_queue, "DONE");
            break;
        }
        usleep(100000);  // Simulate editing by sleeping for 0.1 seconds
        insert_bounded_buffer(ce_args->shared_queue, message);
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
        } else {
            printf("%s\n", message);
        }
    }
    printf("DONE\n");
    return NULL;
}

void read_config(const char *filename, int *num_producers, ProducerArgs **p_args, int *ce_queue_size) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    fscanf(file, "PRODUCER %d\n", num_producers);
    *p_args = (ProducerArgs *)malloc(*num_producers * sizeof(ProducerArgs));

    for (int i = 0; i < *num_producers; i++) {
        fscanf(file, "PRODUCER %d\n", &((*p_args)[i].id));
        fscanf(file, "%d\n", &((*p_args)[i].num_products));
        int queue_size;
        fscanf(file, "queue size = %d\n", &queue_size);
        (*p_args)[i].queue = create_bounded_buffer(queue_size);
    }

    fscanf(file, "Co-Editor queue size = %d\n", ce_queue_size);

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

    BoundedBuffer *dispatcher_queues = (BoundedBuffer *)malloc(NUM_TYPES * sizeof(BoundedBuffer));
    for (int i = 0; i < NUM_TYPES; i++) {
        dispatcher_queues[i] = *create_bounded_buffer(ce_queue_size);
    }

    BoundedBuffer *shared_queue = create_bounded_buffer(ce_queue_size);

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
        p_args[i].queue = create_bounded_buffer(p_args[i].queue->size);
        d_args.producer_queues = p_args[i].queue;
        pthread_create(&producer_threads[i], NULL, producer_thread, &p_args[i]);
    }

    pthread_create(&dispatcher_thread_id, NULL, dispatcher_thread, &d_args);
    for (int i = 0; i < NUM_TYPES; i++) {
        pthread_create(&co_editor_threads[i], NULL, co_editor_thread, &ce_args[i]);
    }
    pthread_create(&screen_manager_thread_id, NULL, screen_manager_thread, &sm_args);

    for (int i = 0; i < num_producers; i++) {
        pthread_join(producer_threads[i], NULL);
    }
    pthread_join(dispatcher_thread_id, NULL);
    for (int i = 0; i < NUM_TYPES; i++) {
        pthread_join(co_editor_threads[i], NULL);
    }
    pthread_join(screen_manager_thread_id, NULL);

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
