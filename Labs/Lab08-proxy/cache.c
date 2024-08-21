#include "cache.h"
#include "csapp.h"

LruCache* cache_create(size_t max_cache_sz, size_t max_object_sz) {
    LruCache *cache = Malloc(sizeof(*cache));
    cache->max_cache_sz = max_cache_sz;
    cache->max_object_sz = max_object_sz;
    cache->cache_sz = 0;
    cache->readcnt = 0;
    Sem_init(&(cache->write), 0, 1);
    Sem_init(&(cache->mutex), 0, 1);

    CacheItem *head = Malloc(sizeof(*head));
    CacheItem *rear = Malloc(sizeof(*rear));
    head->key = NULL;
    head->size = 0;
    head->prev = NULL;
    head->next = rear;
    rear->key = NULL;
    rear->size = 0;
    rear->prev = head;
    rear->next = NULL;
    cache->head = head;
    cache->rear = rear;
    
    return cache;
}

void cacheitem_free(CacheItem *item) {
    Free(item->key);
    Free(item->value);
    Free(item);
}

void cache_free(LruCache* cache) {
    CacheItem *ptr = cache->head;
    while(ptr != NULL) {
        CacheItem *next = ptr->next;
        cacheitem_free(ptr);
        ptr = next;
    }
    Free(cache);
}

// get element from cache by key
CacheItem *cache_index(char *key, LruCache *cache) {
    CacheItem *ptr = cache->head;
    while(ptr != NULL) {
        if (ptr->key != NULL && !strcmp(ptr->key, key)) {
            return ptr;
        }
        ptr = ptr->next;
    }
    return NULL;
}

// unlink element from cache by key
CacheItem *cache_unlink(char *key, LruCache *cache) {
    CacheItem *ptr = cache_index(key, cache);
    if (ptr == NULL) {
        return ptr; // doesn't existed
    }
    CacheItem *prev = ptr->prev;
    CacheItem *next = ptr->next;
    prev->next = next;
    next->prev = prev;

    // update cache metadata
    cache->cache_sz -= ptr->size;

    return ptr;
}

// remove permanently element from cache by key
void cache_remove(char *key, LruCache *cache) {
    // unlink
    CacheItem *item = cache_unlink(key, cache);

    cacheitem_free(item); // free space
    return ;
}

// insert element into the place after head, because the item recently used
void cache_head(CacheItem *item, LruCache *cache) {
    // insert after head
    CacheItem *head = cache->head;
    CacheItem *next = head->next;
    head->next = item;
    next->prev = item;
    item->prev = head;
    item->next = next;

    // update cache metadata
    cache->cache_sz += item->size;

    return ;
}

// evict some item in cache so that there is enough size for insertion of size of sz
void cache_evict(size_t sz, LruCache *cache) {
    CacheItem *head = cache->head;
    CacheItem *ptr = cache->rear->prev;
    CacheItem *prev = NULL;
    size_t f_sz = cache->max_cache_sz - cache->cache_sz;
    while (ptr != head && f_sz >= sz) { // iterate from read to head
        f_sz += ptr->size;
        prev = ptr->prev;
        ptr->next->prev = prev;
        prev->next = ptr->next;
        cacheitem_free(ptr);
        ptr = prev;
    }
    cache->cache_sz = cache->max_cache_sz - f_sz;
}

void cache_insert(char *key, char *value, size_t sz, LruCache *cache) {
    P(&cache->write); // acquire write-lock
    CacheItem *item = cache_index(key, cache);
    if (cache->max_object_sz < sz) {
        if  (item == NULL) {
            // no need to cache this key-value
            V(&cache->write); // release write-lock
            return ;
        } else {
            // remove item from cache
            cache_remove(item->key, cache);
            V(&cache->write); // release write-lock
            return;
        }
    }
    if (item != NULL) {
        cache_unlink(item->key, cache);
    }
    if (cache->max_cache_sz - cache->cache_sz < sz) { // should be after cache_sz-- operation in cache_unlink()
        // need to free some items by EVICTION POLICY for caching this key-value
        cache_evict(sz, cache);
    }
    // insert key-value into cache
    if (item == NULL) {
        item = Malloc(sizeof(*item));
        item->key = key;
        item->value = value;
        item->size = sz;
    }
    // insert item into the head of the double-linked list
    cache_head(item, cache);

    V(&cache->write); // release write-lock
    return ;
}

CacheItem *cache_get(char *key, LruCache *cache) {
    // check if write-lock can be acquired
    P(&cache->mutex);
    cache->readcnt++;
    if (cache->readcnt == 1) {
        P(&cache->write); // acquire write-lock
    }
    V(&cache->mutex);

    CacheItem *item = cache_unlink(key, cache);
    if (item == NULL) {
        // check if write-lock can be released
        P(&cache->mutex);
        cache->readcnt--;
        if (cache->readcnt == 0) {
            V(&cache->write); // release write-lock
        }
        V(&cache->mutex);
        return NULL;
    }
    cache_head(item, cache);

    // check if write-lock can be released
    P(&cache->mutex);
    cache->readcnt--;
    if (cache->readcnt == 0) {
        V(&cache->write); // release write-lock
    }
    V(&cache->mutex);
    
    return item;
}

