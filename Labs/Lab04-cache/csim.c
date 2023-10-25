#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cachelab.h"

/*
For this this lab, you should assume that memory accesses are aligned properly, such that a single
memory access never crosses block boundaries. By making this assumption, you can ignore the
request sizes in the valgrind traces.
*/

# define ADDRESS_SIZE 64
# define EXIT_FAILURE 1

typedef struct {
    int s, E, e, b;
    int SetMask; // the mask used to get set index from address
    int TagMask; // the mask used to get cache line tag from address
    int OffMask; // the mask used to get block offset from address
    CacheLine* Lines; // cacheline array
} CacheSet;

typedef struct {
    int IsValid;
    char* Tag;
    // char* Blocks; // cache block could be ignored
} CacheLine;


int main(int argc, char **argv) {
    int i, j, s =-1, E = -1, b = -1;
    char* title = NULL;
    // read arguments from command line
    for (i = 1; i < argc; i++) {
        if (strncmp(argv[i], "-s", 2) == 0 && i + 1 < argc) {
            s = (int) strtol(argv[++i], NULL, 10);
        } else if (strncmp(argv[i], "-E", 2) == 0 && i + 1 < argc) {
            E = (int) strtol(argv[++i], NULL, 10);
        } else if (strncmp(argv[i], "-b", 2) == 0 && i + 1 < argc) {
            b = (int) strtol(argv[++i], NULL, 10);
        } else if (strncmp(argv[i], "-t", 2) == 0 && i + 1 < argc) {
            title = argv[++i];
        } else {
            printf("invalid argument:%s\n", argv[i]);
            return EXIT_FAILURE;
        }
    }
    // validate arguments
    if (s == -1 || E == -1 || b == -1 || title == NULL) {
        printf("参数少了\n");
        return EXIT_FAILURE;
    }
    if (s + b > 63) { 
        printf("set index 和 block offset 不合理\n");
    }
    // get a simulated cache
    CacheSet* cache = constructCache(s, E, b);
    // read from file specified by -t
    FILE *trace = fopen(title, "r");
    if (trace == NULL) {
        fprintf(stderr, "open file(%s) error\n", title);
        return EXIT_FAILURE;
    }
    simulateCache(trace, cache);
    // close file
    if (fclose(trace) != 0) {
        perror("Failed to close the file!");
        return EXIT_FAILURE;
    }
    return 0;
}

void simulateCache(FILE *input, CacheSet *cache) {
    int hits=0, miss=0, evict=0;
    char *buffer = NULL;
    size_t buf_sz = 0;
    size_t len = 0;
    while ((len = getline(&buffer, &buf_sz, input)) > 0) {
        if (buffer[0] == 'I') {
            continue;
        } else if (buffer[0] != ' ') {
            perror(sprintf("invalid character(%c) in trace file\n", buffer[0]));
            return EXIT_FAILURE;
        }
        int ch, address, sz, tmp;
        if (sscanf(buffer, " %c %x,%d", &ch, &address, &sz) != 3) {
            perror("sscanf error\n");
            return;
        }
        if (ch != 'L' && ch != 'S' && ch != 'M') {
            perror(sprintf("invalid character(%c) in trace file\n", buffer[0]));
            return EXIT_FAILURE;
        }
        if (ch == 'L' || ch == 'M') {
            tmp = loadFromCache(address, cache);
            if (tmp == 0) {
                hits++;
            } else if (tmp == 1) {
                miss++;
            } else {
                evict++;
            }
        } 
        if (ch == 'S' || ch == 'M'){
            tmp = saveIntoCache(address, cache);
            if (tmp == 0) {
                hits++;
            } else if (tmp == 1) {
                miss++;
            } else {
                evict++;
            }
        }
    }
    // print statistic result
    printSummary(hits, miss, evict);
    return ;
}

// return value explain: 0 - hit; 1 - miss; 2 - miss eviction
int loadFromCache(int address, CacheSet *cache) {
    int i;
    int hasEmptyLine=1, emptyLineIdx=0;
    int setIndex = (cache->SetMask & address) >> (cache->e + cache->b);
    int tag = (cache->TagMask & address) >> (cache->b);
    int offset = cache->OffMask & address; // cache block offset is unuseful
    CacheSet cacheSet = cache[setIndex];
    CacheLine *lines = cacheSet.Lines;
    for (i=0; i < cacheSet.E; i++) {
        // hit
        if (lines[i].IsValid == 1 && lines[i].Tag == tag) {
            return 0;
        }
        if (lines[i].IsValid == 0) {
            hasEmptyLine = 0;
            emptyLineIdx = i;
        }
    }
    // miss
    if (hasEmptyLine) {
        lines[emptyLineIdx].IsValid = 1;
        lines[emptyLineIdx].Tag = tag;
        return 1;
    }
    // miss eviction
    
    return 2;
}

int saveIntoCache(int address, CacheSet *cache) {
    // simply simulate
    return loadFromCache(address, cache);
}

// construct the simulator for cache
CacheSet* constructCache(int s, int E, int b) {
    int i, j, e = (ADDRESS_SIZE - s - b);
    if (E != (1L << e)) {
        printf("s=%d, E=%d, e=%d, b=%d invalid\n", s, E, e, b);
        exit(-200);
    }
    long setNum = 1UL << s;
    long blockSize = 1UL << b;
    int setMask, tagMask, offMask;
    // calculate address mask
    offMask = (1UL << b) - 1;
    setMask = (1UL << (b+s)) - 1 - offMask;
    tagMask = ~0 - (setMask | offMask);
    // allocate memory for cache
    CacheSet* cache = malloc(sizeof(*cache)*setNum);
    cache->s = s;
    cache->e = e;
    cache->E = E;
    cache->b = b;
    cache->OffMask = offMask;
    cache->TagMask = tagMask;
    cache->SetMask = setMask;
    for (i=0; i < setNum; i++) {
        CacheLine* cacheLines = malloc(sizeof(*cacheLines)*E);
        // init cache line
        for (j=0; j < E; j++) {
            cacheLines[i].IsValid = 0;
            
        }
        cache[i].Lines = cacheLines;
    }
    return cache;
}