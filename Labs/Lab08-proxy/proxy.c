#include <stdio.h>
#include "csapp.h"
#include "blockqueue.h"
#include "cache.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

// blocked queue size
#define MAX_BQ_SIZE 1024

// worker thread number
#define MAX_WK_NUM 8

// max size of every lines in http
#define MAX_HTTP_LINE 1024

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

// blocked queue for connected fd
BlockQueue *BQ;

// Cache based on LRU, providing thread-safely insert and get method
LruCache *lruCache;

// worker thread, for comsuming BQ, gets a integer argument as connected socket fd
void *worker_thread(void *vargp);
void *worker_task(void *vargp);

// get an entire http request from socket fd, and return the request-line in http
char **get_http_request(int fd);

// get hostname from http request line
void gethostnamefromhttp(char *src, char *method, char *host, char *port, char *uri);

// send an http request
int send_http_request(int fd, char **httpreq);

// redirect http response
int redirect_http_response(int srcfd, int desfd, char *cache_key);

// free space of string array
void free_str_arr(char **arr);

int main(int argc, char **argv)
{
    int i, listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    if (argc != 2) {
        perror("usage error");
        exit(0);
    }

    // 1.initialize shared blocked queue and cache
    BQ = bq_init(MAX_BQ_SIZE);
    lruCache = cache_create(MAX_CACHE_SIZE, MAX_OBJECT_SIZE);

    // 2.initialize the worker thread (create pthreads that get task from MyTaskQueue and finish it)
    for (i=0; i<MAX_WK_NUM; i++) {
        Pthread_create(&tid, NULL, worker_thread, NULL);
    }

    // 3.Listening a specified port
    listenfd = Open_listenfd(argv[1]); // get and set a listening socket
    while(1) { // produce task
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        // 4.abstract user request into MyTask structure and insert into MyTaskQueue (the only one producer thread)
        bq_add(BQ, connfd);
    }

    // free shared blocked queue
    bq_clear(BQ);
    cache_free(lruCache);
    return 0;
}

void *worker_thread(void *vargp) {
    while(1) {
        worker_task(vargp);
    }
}

void *worker_task(void *vargp) {
    int connfd, clientfd;
    char **old_req;
    char hostname[MAXLINE], port[MAXLINE], method[MAXLINE], uri[MAXLINE];
    // 0.get an task from BQ
    connfd = bq_get(BQ);

    // 1.wait and read an entire HTTP request
    old_req = get_http_request(connfd);
    // 2.check if cache-hit
    CacheItem *cacheItem = cache_get(old_req[0], lruCache);
    if (cacheItem != NULL) {
        // cache hitting
        Rio_writen(connfd, cacheItem->value, cacheItem->size);
        free_str_arr(old_req);
        return 0;
    }
    // cache missiing
    char *cache_key = Malloc(sizeof(*cache_key)*MAXLINE); // get cache key
    strcpy(cache_key, old_req[0]);
    gethostnamefromhttp(old_req[0], method, hostname, port, uri);
    free_str_arr(old_req);
    if (strncmp("GET", method, 3)) {
        printf("invalid http method\n");
        return 0;
    }

    // 2.add some HTTP head
    char **new_req = Malloc(sizeof(*new_req)*6);
    new_req[0] = Malloc(sizeof(*new_req[0])*MAX_HTTP_LINE);
    sprintf(new_req[0], "GET /%s HTTP/1.0\r\n\r\n", uri);
    new_req[1] = Malloc(sizeof(*new_req[1])*22);
    sprintf(new_req[1], "Connection: close\r\n");
    new_req[2] = Malloc(sizeof(*new_req[2])*28);
    sprintf(new_req[2], "Proxy-Connection: close\r\n");
    new_req[3] = Malloc(sizeof(*new_req[3])*128);
    strcpy(new_req[3], user_agent_hdr); // copy user_agent_hdr, including the terminating null byte
    new_req[4] = Malloc(sizeof(*new_req[4])*MAX_HTTP_LINE);
    sprintf(new_req[4], "Host: %s\r\n\r\n", hostname);
    new_req[5] = NULL; // null-terminated

    // 3.request with new HTTP request
    if ((clientfd = open_clientfd(hostname, port)) < 0) {
        free_str_arr(new_req);
        // Close(clientfd);
        Close(connfd);
        printf("open_clientfd fail\n");
        return 0;
    }
    if (send_http_request(clientfd, new_req)) {
        free_str_arr(new_req);
        Close(clientfd);
        Close(connfd);
        printf("send_http_request fail\n");
        return 0;
    }
    free_str_arr(new_req);

    // 4.redirect response to client
    if (redirect_http_response(clientfd, connfd, cache_key)) {
        printf("redirect_http_response fail\n");
    }

    // release source
    Close(clientfd);
    Close(connfd);
    return 0;
}

char **get_http_request(int fd) {
    char **req;
    size_t req_sz = 1;
    ssize_t sz;
    rio_t rbuf; // internal read buffer, stored on stack instead of heap
    char httptext[MAX_HTTP_LINE];
    int termi_cnt = 0; // count "\r\n" http section barrier textline

    Rio_readinitb(&rbuf, fd); // init internal read buffer

    req = Malloc(sizeof(req)*req_sz);
    req[0] = NULL; // null terminated array
    while((sz = Rio_readlineb(&rbuf, &httptext, MAX_HTTP_LINE)) > 0) {
        req = Realloc(req, sizeof(req)*(req_sz + 1));
        req[req_sz-1] = Malloc(sizeof(*req)*MAX_HTTP_LINE);
        strncpy(req[req_sz-1], httptext, sz);
        req_sz++;
        if (strncmp(httptext, "\r\n", 2)) {
            termi_cnt++;
        }
        if (termi_cnt >= 2) {
            break; // already get an entire http GET request
        }
    }
    req[req_sz-1] = NULL; // null terminated array

    return req;
}

int send_http_request(int fd, char **httpreq) {
    int i;
    for (i=0; httpreq[i]; i++) {
        Rio_writen(fd, httpreq[i], strlen(httpreq[i]));
    }
    return 0;
}

int redirect_http_response(int srcfd, int desfd, char *cache_key) {
    char buf[MAXLINE];
    ssize_t rsz, cache_sz=0;
    rio_t rio;
    char *cache_buf = Malloc(sizeof(cache_buf)*MAX_OBJECT_SIZE);

    Rio_readinitb(&rio, srcfd);
    while((rsz = Rio_readnb(&rio, buf, MAX_HTTP_LINE)) > 0) {
        Rio_writen(desfd, buf, rsz);
        if (cache_sz + rsz <= MAX_OBJECT_SIZE) {
            strncpy(cache_buf+cache_sz, buf, rsz);
        }
        cache_sz += rsz;
    }

    // cache this http response
    cache_insert(cache_key, cache_buf, cache_sz, lruCache);
    return 0;
}

// GET http://localhost:15213/home.html HTTP/1.1
void gethostnamefromhttp(char *src, char *method, char *host, char *port, char *uri) {
    char tmp[MAXLINE], version[MAXLINE];
    sscanf(src, "%s http://%s %s", method, tmp, version);

    int i=0, j=0, l=strlen(tmp)+1;
    while(tmp[i]) {
        if (tmp[i] == ':') {
            strncpy(host, tmp, i - j);
            host[i - j] = '\0';
            j = i+1;
        }
        if (tmp[i] == '/') {
            if (j == 0) {
                // no port
                strncpy(host, tmp, i - j);
                strcpy(port, "80");
            } else {
                strncpy(port, &tmp[j], i - j);
            }
            port[i - j] = '\0';
            break;
        }
        i++;
    }
    strncpy(uri, &tmp[i+1], l - i - 1);
    return;
}

void free_str_arr(char **arr) {
    char **sta = arr;
    while (arr != NULL && *arr != NULL) {
        Free(*arr);
        arr++;
    }
    Free(sta);
}