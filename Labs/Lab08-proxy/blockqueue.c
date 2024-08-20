#include "blockqueue.h"
#include "csapp.h"

// construct and initialize blocked queue
BlockQueue* bq_init(int n) {
    BlockQueue *bq = (BlockQueue *) Malloc(sizeof(*bq)); // allocating on heap
    int *buf = (int *) Malloc(sizeof(*buf) * n);
    bq->buf = buf;
    bq->n = n;
    bq->front = 0;
    bq->rear = 0;
    Sem_init(&(bq->mutex), 0, 1);
    Sem_init(&(bq->slots), 0, n);
    Sem_init(&(bq->items), 0, 0);
    return bq;
}

// reap blocked queue
void bq_clear(BlockQueue *bq) {
    Free(bq->buf);
    Free(bq);
}

// get one first item from blocked queue; if no item, blocked current pthread
int bq_get(BlockQueue *bq) {
    int t;

    P(&(bq->items));    // if no item, blocking current pthread; else proceeding on
    P(&(bq->mutex));    // mutex all threads 
    t = bq->buf[bq->front % (bq->n)];
    bq->front = (bq->front + 1) % (bq->n); // point to next item in round array
    V(&(bq->mutex));    // release mutex lock
    V(&(bq->slots));    // annouce available slot, awaken a producer thread that blocked by this semaphore

    return t; 
}

// add one item into the rear of blocked queue; if no slot, blocked current pthread
void bq_add(BlockQueue *bq, int val) {
    P(&(bq->slots));    // if no slot, blocking current pthread; else proceeding on
    P(&(bq->mutex));    // mutex all threads 
    bq->buf[bq->rear] = val;
    bq->rear = (bq->rear + 1) % (bq->n);
    V(&(bq->mutex));    // release mutex lock
    V(&(bq->items));    // annouce available item, awaken a comsumer thread that blocked by this semaphore
}