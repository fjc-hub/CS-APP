#include <semaphore.h>

typedef struct CacheItem_t {
    char *key; // unique key in cache, using to index
    char *value;
    struct CacheItem_t *prev;
    struct CacheItem_t *next;
    size_t size; // the size of the payload of value
} CacheItem;

typedef struct {
    CacheItem *head; // header of the double linked list
    CacheItem *rear; // tailer of the double linked list
    size_t cache_sz; // cache's payload size
    size_t max_cache_sz;
    size_t max_object_sz;
    int readcnt; // count for current reader thread
    sem_t write; // write lock
    sem_t mutex; // mutex lock for protecting accesses for readcnt
} LruCache;

LruCache* cache_create(size_t max_cache_sz, size_t max_object_sz);

void cache_free(LruCache* cache);

void cache_insert(char *key, char *value, size_t sz, LruCache *cache);

CacheItem *cache_get(char *key, LruCache *cache);