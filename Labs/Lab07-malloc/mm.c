/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Dream of China",
    /* First member's full name */
    "fujiacheng",
    /* First member's email address */
    "fjc2319@gmail.com",
    "",
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define MIN_FREE_BLOCK_SZ 32 // 2 * ALIGNMENT + 2 * SIZE_T_SIZE; minimun size of free block

#define MAX(a,b) (((a)>(b))?(a):(b))

// get value pointed to by void pointer
#define GET(p) (*(unsigned long *)(p))
// set val to the location pointed to by void pointer
#define PUT(p, val) (*(unsigned long *)(p) = (unsigned long) (val))
// get the size of free block
#define GET_SIZE(p) (*(size_t *)(p) & ~0x7)

// arithmatic of void pointer
#define VOID_ADD(p, x) ((char *)(p) + (x))
#define VOID_DEL(p, x) ((char *)(p) - (x))

void *HEADER; // head block base-address of the heap
void *TAILER; // tail block base-address of the heap

int mm_check_free_list(); // check function
void rm_free_node_from_list(void *ptr);  // remove a free block node from implicit free list
void insert_free_node_into_list(void *ptr); // insert a free block node into implicit free list, inserted node is next to HEADER

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    // init heap
    mem_init();

    // init explicit free list, construct dummy header and tailer nodes
    if (mem_sbrk(ALIGN(3 * SIZE_T_SIZE + 4 * ALIGNMENT)) == (void *) -1) {
        printf("mem_sbrk fail");
        return -1;
    }
    // double linked list header <=> tailer
    void *first = mem_heap_lo();
    void *last = mem_heap_hi();

    size_t tail_blk_sz = ALIGN(SIZE_T_SIZE + 2 * ALIGNMENT);
    void *tailer_start = VOID_DEL(last, tail_blk_sz - 1);
    PUT(VOID_DEL(last, ALIGNMENT - 1), 0); //init tailer node's next pointer
    PUT(tailer_start, 0x3); // init tailer node's size to 0, and prev block is allocated

    PUT(first, 0x1); // init header node's size to 0 and alloc-control-bit to 1 
    PUT(VOID_ADD(first, SIZE_T_SIZE), 0); // init header node's prev pointer
    PUT(VOID_ADD(first, ALIGNMENT + SIZE_T_SIZE), tailer_start); // init header node's next pointer
    PUT(VOID_ADD(first, ALIGNMENT + 2 * SIZE_T_SIZE), 0x1); // init header node's footer
   
    PUT(VOID_ADD(tailer_start, SIZE_T_SIZE), first); // init tailer node's prev node pointer
    PUT(VOID_ADD(first, ALIGNMENT + SIZE_T_SIZE), tailer_start); // init header node's next node pointer

    HEADER = first;
    TAILER = tailer_start;

    return 0;
}

/*
  header and footer layout:
    +----------- 64 ------------+
    |     block size    | 0 0 1 |
    +---------------------------+
    the lowest 3 bits is control-bit:
    1.first bit presents if this block is allocated
    2.second bit presents if the previous block is allocated
    3.third bit retains
*/

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }

    // adjust size to align ALIGNMENT bytes
    size_t resize = MAX(MIN_FREE_BLOCK_SZ, ALIGN(size + SIZE_T_SIZE)); // allocated block has no footer, only header

    // look through explicit free list
    void *curr = HEADER;
    while(curr != NULL) {
        size_t curr_sz = GET_SIZE(curr);
        if (resize <= curr_sz) {
            // match
            // split curr free block if necessary
            if (resize + MIN_FREE_BLOCK_SZ <= curr_sz) { // split it and use lower-bit part as allocated block 
                // 1.update old free block's meta-data
                void *prev_node_ptr = (void *) GET((void *) VOID_ADD(curr, SIZE_T_SIZE)); // get previous free node pointer
                void *next_node_ptr = (void *) GET((void *) VOID_ADD(curr, SIZE_T_SIZE + ALIGNMENT)); // get next free node pointer
                void *old_ptr = (void *) VOID_ADD(curr, resize); // use second part as old free block
                PUT(old_ptr, (curr_sz - resize) | 0x2); // set header
                PUT(VOID_ADD(old_ptr, SIZE_T_SIZE), prev_node_ptr); // set previous free node pointer
                PUT(VOID_ADD(old_ptr, SIZE_T_SIZE + ALIGNMENT), next_node_ptr); // set next free node pointer
                PUT(VOID_ADD(old_ptr, curr_sz - resize - SIZE_T_SIZE), (curr_sz - resize) | 0x2); // set footer
                PUT(VOID_ADD(prev_node_ptr, SIZE_T_SIZE + ALIGNMENT), old_ptr); // set previous-free-block's next pointer
                PUT(VOID_ADD(next_node_ptr, SIZE_T_SIZE), old_ptr); // set next-free-block's prev pointer 

                // 2.construct allocated block
                unsigned long old_sz_bit_mask = (GET(curr) & 0x6); // get old 2 control bits
                PUT(curr, (resize | old_sz_bit_mask) | 0x1); // set header's size and remain old control-bit

                // 3.return the base address of payload 
                return (void *) VOID_ADD(curr, SIZE_T_SIZE);
            } else { // not split and use whole block as allocated block (剩余的空闲块大小不满足MIN_FREE_BLOCK_SZ条件，存不下指向前后空闲块的指针)
                // 1.update header
                void *next_block_ptr = (void *) VOID_ADD(curr, curr_sz);
                PUT(next_block_ptr, GET(next_block_ptr) | 0x2); // update next block's header
                PUT(curr, GET(curr) | 0x1); // update this block's header

                // 2.remove curr block from implicit free list
                rm_free_node_from_list(curr);

                // 3.return the base address of payload 
                return  (void *) VOID_ADD(curr, SIZE_T_SIZE);
            }
        } else {
            // check next block
            unsigned long next = GET(VOID_ADD(curr, SIZE_T_SIZE + ALIGNMENT));
            if (next == 0) {
                // no match blocks
                break;
            }
            curr = (void *) next;
        }
    }

    // no capable free block, expanding heap
    void *p = mem_sbrk(resize);
    if (p == (void *) -1)
	    return NULL;
    else {
        // 1.move tailer block to the end of physical heap
        void *tailer = TAILER;
        void *last = mem_heap_hi();
        void *new_tail = VOID_DEL(last, ALIGN(SIZE_T_SIZE + 2 * ALIGNMENT) - 1);
        void *tailer_prev_node_ptr = (void *) GET((void *) VOID_ADD(tailer, SIZE_T_SIZE)); // tailer block's prev free node

        PUT(new_tail, GET(tailer) | 0x3); // set new tailer block's control-bit and size
        PUT(VOID_ADD(new_tail, SIZE_T_SIZE), tailer_prev_node_ptr); // set new tailer block's prev-pointer
        PUT(VOID_ADD(new_tail, SIZE_T_SIZE + ALIGNMENT), NULL); // set new tailer block's next-pointer

        // 2.construct allocated block
        unsigned long new_control_bit = GET(tailer) & 0x7;
        new_control_bit = new_control_bit | 0x1; // set first control-bit to 1
        PUT(tailer, resize | new_control_bit); // set allocated block's header

        // 3.update TAILER
        TAILER = new_tail;

        // 4.update previous free block (of tailer) 's next node pointer
        PUT(VOID_ADD(tailer_prev_node_ptr, SIZE_T_SIZE + ALIGNMENT), new_tail);

        // 5.return the address of payload
        return (void *) VOID_ADD(tailer, SIZE_T_SIZE);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    // mm_check_free_list();

    if (ptr == NULL) 
        return;
    
    void *block_ptr = VOID_DEL(ptr, SIZE_T_SIZE); // block pointer
    unsigned long pre_blk_alloc = GET(block_ptr) & 0x2; // flag presenting if physically previous block is allocated 
    unsigned long block_sz = GET_SIZE(block_ptr); // block size
    void *next_block_ptr = (void *) VOID_ADD(block_ptr, block_sz); // next block's pointer
    unsigned long nxt_blk_alloc = GET(next_block_ptr) & 0x1; // flag presenting if physically next block is allocated

    // check if block ptr can be coalesced and merge consecutive free blocks
    void *new_block_ptr = block_ptr;
    size_t new_block_size = (size_t) GET_SIZE(block_ptr);
    if (pre_blk_alloc) {
        if (nxt_blk_alloc) {
            // case 1: prev block and next block are both allocated
            PUT(new_block_ptr, new_block_size | 0x2); // set header of new free block
            PUT(VOID_ADD(new_block_ptr, new_block_size-SIZE_T_SIZE), new_block_size | 0x2); // set footer of new free block
            PUT(next_block_ptr, GET(next_block_ptr) & ~0x2); // set next block's control bit, cause its prev block is free

            insert_free_node_into_list(new_block_ptr); // insert new_block_ptr to the implicit free list
        } else {
            // case 2: prev block is allocated, next block is free
            void *nxt_prev_node_ptr = (void *) GET(VOID_ADD(next_block_ptr, SIZE_T_SIZE)); // get next block's prev node
            void *nxt_next_node_ptr = (void *) GET(VOID_ADD(next_block_ptr, SIZE_T_SIZE + ALIGNMENT)); // get next block's next node
            
            PUT(VOID_ADD(nxt_prev_node_ptr, SIZE_T_SIZE + ALIGNMENT), (unsigned long) new_block_ptr); // set prev node's next node
            PUT(VOID_ADD(nxt_next_node_ptr, SIZE_T_SIZE), (unsigned long) new_block_ptr); // set next node's prev node

            new_block_size += GET_SIZE(next_block_ptr);
            
            PUT(VOID_ADD(new_block_ptr, SIZE_T_SIZE), (unsigned long) nxt_prev_node_ptr); // set prev node of new free block
            PUT(VOID_ADD(new_block_ptr, SIZE_T_SIZE + ALIGNMENT), (unsigned long) nxt_next_node_ptr); // set next node of new free block
            PUT(new_block_ptr, new_block_size | 0x2); // set header
            PUT(VOID_ADD(new_block_ptr, new_block_size-SIZE_T_SIZE), new_block_size | 0x2); // set footer
        }
    } else {
        if (nxt_blk_alloc) {
            // case 3: prev block is free, next block is allocated
            void *prev_block_footer = (void *) VOID_DEL(new_block_ptr, SIZE_T_SIZE);
            void *prev_block_ptr = (void *) VOID_DEL(new_block_ptr, GET_SIZE(prev_block_footer)); // previous block's address
            void *new_block_footer = (void *) VOID_DEL(next_block_ptr, SIZE_T_SIZE); // new block's new footer

            new_block_ptr = prev_block_ptr; // change block pointer
            new_block_size += GET_SIZE(prev_block_ptr);

            unsigned long prev_block_control_bit = GET(prev_block_ptr) & 0x6; // get previous block's address
            PUT(prev_block_ptr, new_block_size | prev_block_control_bit); // update new block's header
            PUT(new_block_footer, new_block_size | prev_block_control_bit); // update new block's footer

            PUT(next_block_ptr, GET(next_block_ptr) & ~0x2); // update next block's header
        } else {
            // case 4: prev is free, next is free
            void *prev_block_footer = (void *) VOID_DEL(new_block_ptr, SIZE_T_SIZE);
            void *prev_block_ptr = (void *) VOID_DEL(new_block_ptr, GET_SIZE(prev_block_footer)); // previous block's address
            void *next_block_ptr = (void *) VOID_ADD(new_block_ptr, new_block_size); // next block's address
            void *next_block_footer = (void *) VOID_ADD(next_block_ptr, GET_SIZE(next_block_ptr) - SIZE_T_SIZE); // new block's footer

            rm_free_node_from_list(prev_block_ptr); // remove free block node
            rm_free_node_from_list(next_block_ptr); // remove free block node

            new_block_ptr = prev_block_ptr; // change block pointer
            new_block_size += (GET_SIZE(prev_block_ptr) + GET_SIZE(next_block_ptr));

            unsigned long prev_block_control_bit = GET(prev_block_ptr) & 0x6; // get previous block's address
            PUT(prev_block_ptr, new_block_size | prev_block_control_bit); // update new block's header
            PUT(next_block_footer, new_block_size | prev_block_control_bit); // update new block's footer

            insert_free_node_into_list(new_block_ptr); // insert into free list
        }
    }

}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    
    if (size == 0) {
        mm_free(ptr);
        return (void *) 0;
    }

    if (oldptr == NULL) {
        return mm_malloc(size);
    }
    
    // check if the ptr block has enough payload for size
    size_t new_sz = MAX(MIN_FREE_BLOCK_SZ, ALIGN(size + SIZE_T_SIZE));
    size_t old_sz = GET_SIZE((void *) VOID_DEL(ptr, SIZE_T_SIZE));
    if (old_sz >= new_sz) {
        return ptr;
    }
    
    // get a new free block 
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    
    memcpy(newptr, oldptr, old_sz);
    mm_free(oldptr);
    return newptr;
}

void rm_free_node_from_list(void *ptr) {
    void *prev_node_ptr = (void *) GET(VOID_ADD(ptr, SIZE_T_SIZE));
    void *next_node_ptr = (void *) GET(VOID_ADD(ptr, SIZE_T_SIZE + ALIGNMENT));

    PUT(VOID_ADD(prev_node_ptr, SIZE_T_SIZE + ALIGNMENT), (unsigned long) next_node_ptr);
    PUT(VOID_ADD(next_node_ptr, SIZE_T_SIZE), (unsigned long) prev_node_ptr);
}

void insert_free_node_into_list(void *ptr) {
    void *head_ptr = HEADER;
    void *head_next_node_ptr = (void *) GET(VOID_ADD(head_ptr, SIZE_T_SIZE + ALIGNMENT));

    PUT(VOID_ADD(head_ptr, SIZE_T_SIZE + ALIGNMENT), (unsigned long) ptr); // set header block's next node
    PUT(VOID_ADD(head_next_node_ptr, SIZE_T_SIZE), (unsigned long) ptr); // set prev node
    
    PUT(VOID_ADD(ptr, SIZE_T_SIZE), (unsigned long) head_ptr); // set prev node of new free block
    PUT(VOID_ADD(ptr, SIZE_T_SIZE + ALIGNMENT), (unsigned long) head_next_node_ptr); // set next node of new free block
}

int mm_check_free_list() {
    void *curr = HEADER;
    curr = (void *) GET(VOID_ADD(curr, SIZE_T_SIZE + ALIGNMENT)); // skip HEADER
    while(curr != NULL && curr != TAILER) {
        size_t sz = GET_SIZE(curr);
        unsigned long is_alloc = GET(curr) & 0x1;
        if (sz > 0 && is_alloc) {
            printf("non-free node in implicit free list\n");
            exit(-1);
        }
        unsigned long is_pre_alloc = GET(curr) & 0x2;
        if (!is_pre_alloc) {
            unsigned long pre_sz = GET_SIZE(VOID_DEL(curr, SIZE_T_SIZE));
            if (pre_sz > 0) {
                printf("free block's prev block is free and not coalesced\n");
                return -1;
            }
        }
        unsigned long is_nxt_alloc = GET(VOID_ADD(curr, sz)) &0x1;
        if (!is_nxt_alloc) {
            unsigned long nxt_sz = GET_SIZE(VOID_ADD(curr, sz));
            if (nxt_sz > 0) {
                printf("free block's next block is free and not coalesced\n");
                exit(-1);
            }
        }
        // next node
        curr = (void *) GET(VOID_ADD(curr, SIZE_T_SIZE + ALIGNMENT));
    }
    return -1;
}
