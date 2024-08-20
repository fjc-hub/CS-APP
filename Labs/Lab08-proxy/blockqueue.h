#include <semaphore.h>

typedef struct {
    int *buf;       // buffer item array, it's a round array
    int n;          // buffer size
    int front;      // buf[front % n] indexes the first item
    int rear;       // buf[(rear - 1) % n] indexes the last item
    sem_t mutex;    /* mutex for sync all producer-threads and cpmsumer-threads
                        protect accesses to shared buf,front and rear variable */
    sem_t slots;    // Counts available slots(semaphore for syncing all producers)
    sem_t items;    // Counts available items(semaphore for syncing all comsumers)
} BlockQueue;

// construct and initialize blocked queue
BlockQueue* bq_init(int n);

// reap blocked queue
void bq_clear(BlockQueue *bq);

// get one item from blocked queue
int bq_get(BlockQueue *bq);

// add one item into blocked queue
void bq_add(BlockQueue *bq, int val);