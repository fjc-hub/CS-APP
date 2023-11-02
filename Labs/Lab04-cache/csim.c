#define _POSIX_C_SOURCE 200809L

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

const char * USAGE = "Usage: ./csim-ref [-hv] -s <num> -E <num> -b <num> -t <file>\nOptions:\n  -h         Print this help message.\n  -v         Optional verbose flag.\n  -s <num>   Number of set index bits.\n  -E <num>   Number of lines per set.\n  -b <num>   Number of block offset bits.\n  -t <file>  Trace file.\n\nExamples:\n  linux>  ./csim-ref -s 4 -E 1 -b 4 -t traces/yi.trace\n  linux>  ./csim-ref -v -s 8 -E 2 -b 4 -t traces/yi.trace\n";

typedef struct {
    int IsValid;
    int Tag;
    long int Age; // used to LRU; there is a risk of overflow
    // char* Blocks; // cache block could be ignored
} CacheLine;

typedef struct {
    int Associativity; // the length of Lines    
    CacheLine** Lines; // cacheline array
} CacheSet;

typedef struct {
    int s, E, b;
    int SetMask; // the mask used to get set index from address
    int TagMask; // the mask used to get cache line tag from address
    int OffMask; // the mask used to get block offset from addressLR
    CacheSet** Sets; // cache set array
} Cache;

// free malloced memory
void free_CacheSet(CacheSet* cacheSet) {
    if (cacheSet == NULL) {
        return;
    }
    for (int i=0; i < cacheSet->Associativity; i++) {
        free(cacheSet->Lines[i]);
    }
    free(cacheSet->Lines); // important free!!!
    free(cacheSet);
}

// free malloced memory
void free_Cache(Cache* cache) {
    if (cache == NULL) {
        return;
    }
    for (int i=0; i < cache->E; i++) {
        free_CacheSet(cache->Sets[i]);
    }
    free(cache->Sets); // important free!!!
    free(cache);
}

// construct the simulator for cache
Cache* constructCache(int s, int E, int b) {
    int i, j;
    long setNum = 1UL << s;
    int setMask, tagMask, offMask;
    // calculate address mask
    offMask = (1UL << b) - 1;
    setMask = (1UL << (b+s)) - 1 - offMask;
    tagMask = ~0 - (setMask | offMask);
    // allocate memory for cache
    Cache* cache = malloc(sizeof(*cache));
    cache->s = s;
    cache->E = E;
    cache->b = b;
    cache->OffMask = offMask;
    cache->TagMask = tagMask;
    cache->SetMask = setMask;
    cache->Sets = malloc(sizeof(*cache->Sets)*setNum);
    for (i=0; i < setNum; i++) {
        CacheSet* cacheSet = malloc(sizeof(*cacheSet));
        cacheSet->Lines = malloc(sizeof(*cacheSet->Lines)*E);
        // init cache line
        for (j=0; j < E; j++) {
            CacheLine* cacheLine = malloc(sizeof(*cacheLine));
            cacheLine->IsValid = 0;
            cacheLine->Age = 0;
            cacheSet->Lines[j] = cacheLine;
        }
        // init cache set
        cacheSet->Associativity = E;
        cache->Sets[i] = cacheSet;
    }
    return cache;
}

// return value explain: 0 - hit; 1 - miss; 2 - miss eviction
int loadFromCache(int address, Cache *cache, long int counter) {
    int i;
    long minAge = counter;
    int minIdx = -1;
    int setIndex = (cache->SetMask & address) >>  cache->b;
    int tag = (cache->TagMask & address) >> (cache->s + cache->b);
    // int offset = cache->OffMask & address; // cache block offset is unuseful
    CacheSet* cacheSet = cache->Sets[setIndex];
    CacheLine* *lines = cacheSet->Lines;
    // loop through cache lines in a cache set
    for (i=0; i < cacheSet->Associativity; i++) {
        // hit
        if (lines[i]->IsValid == 1 && lines[i]->Tag == tag) {
            lines[i]->Age = counter;
            return 0;
        }
        // miss
        if (lines[i]->IsValid == 0) {
            lines[i]->IsValid = 1;
            lines[i]->Tag = tag;
            lines[i]->Age = counter;
            return 1;
        }
        if (lines[i]->Age < minAge) {
            minAge = lines[i]->Age;
            minIdx = i;
        }
    }
    // miss eviction
    lines[minIdx]->Tag = tag;
    lines[minIdx]->Age = counter;
    return 2;
}

int saveIntoCache(int address, Cache *cache, long int counter) {
    // simply simulate
    return loadFromCache(address, cache, counter);
}

int simulateCache(FILE *input, Cache *cache, int verbose) {
    int hits=0, miss=0, evict=0;
    char *load_str = "", *save_str = "";
    char buffer[20];
    long int counter = 0;
    while (fgets(buffer, sizeof(buffer), input) != NULL) {
        load_str = "", save_str = "";
        if (buffer[0] == 'I') {
            continue;
        } else if (buffer[0] != ' ') {
            printf("invalid character(%c) in trace file\n", buffer[0]);
            return EXIT_FAILURE;
        }
        char ch;
        int address, sz, tmp;
        if (sscanf(buffer, " %c %x,%d", &ch, &address, &sz) != 3) {
            printf("sscanf error\n");
            return EXIT_FAILURE;
        }
        if (ch != 'L' && ch != 'S' && ch != 'M') {
            printf("invalid character(%c) in trace file\n", buffer[0]);
            return EXIT_FAILURE;
        }
        if (ch == 'L' || ch == 'M') {
            tmp = loadFromCache(address, cache, counter);
            if (tmp == 0) {
                load_str = " hit";
                hits++;
            } else if (tmp == 1) {
                load_str = " miss";
                miss++;
            } else {
                load_str = " miss eviction";
                miss++;
                evict++;
            }
        } 
        if (ch == 'S' || ch == 'M'){
            tmp = saveIntoCache(address, cache, counter);
            if (tmp == 0) {
                save_str = " hit";
                hits++;
            } else if (tmp == 1) {
                save_str = " miss";
                miss++;
            } else {
                save_str = " miss eviction";
                miss++;
                evict++;
            }
        }
        if (verbose) {
            printf("%c %x,%d%s%s\n", ch, address, sz, load_str, save_str);
        }
        counter++;
    }
    // print statistic result
    printSummary(hits, miss, evict);
    return 0;
}

int main(int argc, char **argv) {
    int i, s =-1, E = -1, b = -1, verbose = 0;
    char* title = NULL;
    // read arguments from command line
    for (i = 1; i < argc; i++) {
        if (strncmp(argv[i], "-h", 2) == 0) {
            printf("%s", USAGE);
            return 0;
        } else if (strncmp(argv[i], "-v", 2) == 0) {
            verbose = 1;
        } else if (strncmp(argv[i], "-s", 2) == 0) {
            s = (int) strtol(argv[++i], NULL, 10);
        } else if (strncmp(argv[i], "-E", 2) == 0) {
            E = (int) strtol(argv[++i], NULL, 10);
        } else if (strncmp(argv[i], "-b", 2) == 0) {
            b = (int) strtol(argv[++i], NULL, 10);
        } else if (strncmp(argv[i], "-t", 2) == 0) {
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
    Cache* cache = constructCache(s, E, b);
    // read from file specified by -t
    FILE *trace = fopen(title, "r");
    if (trace == NULL) {
        fprintf(stderr, "open file(%s) error\n", title);
        return EXIT_FAILURE;
    }
    simulateCache(trace, cache, verbose);
    // close file
    if (fclose(trace) != 0) {
        printf("Failed to close the file!\n");
        return EXIT_FAILURE;
    }
    // free memory
    free_Cache(cache);
    return 0;
}
