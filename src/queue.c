#include "queue.h"

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#define handle_error_en(en, msg) \
        do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define handle_error(msg) \
        do { perror(msg); exit(EXIT_FAILURE); } while (0)


/*
 * Queue - the abstract type of a concurrent queue.
 * You must provide an implementation of this type 
 * but it is hidden from the outside.
 */
typedef struct QueueStruct {
    sem_t full;     //To control threads read and write from queue
    sem_t empty;    //To control threads read and write from queue
    void **task;    //pointer to task which is wait for proceed
    size_t size;    //Queue size
    pthread_mutex_t mutex;   //pretect critical resource
} Queue;


/**
 * Allocate a concurrent queue of a specific size
 * @param size - The size of memory to allocate to the queue
 * @return queue - Pointer to the allocated queue
 */
Queue *queue_alloc(int size) {
    // assert(0 && "not implemented yet!");
    Queue* queue = (Queue*)malloc(sizeof(Queue));
    queue->task = (void **)malloc(sizeof(void**) * size);
    queue->size = 0;
    sem_init(&queue->full, 0, 0);
    sem_init(&queue->empty, 0, size);
    pthread_mutex_init(&queue->mutex, NULL);

    return queue;
}


/**
 * Free a concurrent queue and associated memory 
 *
 * Don't call this function while the queue is still in use.
 * (Note, this is a pre-condition to the function and does not need
 * to be checked)
 * 
 * @param queue - Pointer to the queue to free
 */
void queue_free(Queue *queue) {
    // assert(0 && "not implemented yet!");
    free(queue->task);
    free(queue);
}


/**
 * Place an item into the concurrent queue.
 * If no space available then queue will block
 * until a space is available when it will
 * put the item into the queue and immediatly return
 *  
 * @param queue - Pointer to the queue to add an item to
 * @param item - An item to add to queue. Uses void* to hold an arbitrary
 *               type. User's responsibility to manage memory and ensure
 *               it is correctly typed.
 */
void queue_put(Queue *queue, void *item) {
    // assert(0 && "not implemented yet!");

    //When there are still have empty for item, decrease empty semaphore
    sem_wait(&queue->empty);
    pthread_mutex_lock(&queue->mutex);

    //Increase size and add item into last position
    queue->task[queue->size] = item;
    queue->size++;

    //After item put into queue, unlock and increase full semaphore
    pthread_mutex_unlock(&queue->mutex);
    sem_post(&queue->full);
}


/**
 * Get an item from the concurrent queue
 * 
 * If there is no item available then queue_get
 * will block until an item becomes avaible when
 * it will immediately return that item.
 * 
 * @param queue - Pointer to queue to get item from
 * @return item - item retrieved from queue. void* type since it can be 
 *                arbitrary 
 */
void *queue_get(Queue *queue) {
    // assert(0 && "not implemented yet!");

    //When there are still have item for consume, decrease full semaphore
    sem_wait(&queue->full);
    pthread_mutex_lock(&queue->mutex);

    //Get the first item in the queue
    void** task = queue->task[0];

    //Move each item in the queue forward
    for(int i = 1; i < queue->size; i++){
        queue->task[i - 1] = queue->task[i];
    }

    //Subtract size and set last item into NULL to keep queue data correct 
    queue->size--;
    queue->task[queue->size] = NULL;

    //After item get from queue, unlock and increase empty semaphore
    pthread_mutex_unlock(&queue->mutex);
    sem_post(&queue->empty);

    return task;
}

