/* *
 *@file  Explores memory management at the C runtime level.
 *
 * Author: Bharath Gajjala
 *
 * To use (one specific command):
 * LD_PRELOAD=$(pwd)/allocator.so command
 * ('command' will run with your allocator)
 *
 * To use (all following commands):
 * export LD_PRELOAD=$(pwd)/allocator.so
 * (Everything after this point will use your custom allocator -- be careful!)
 */
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "allocator.h"
#include "logger.h"

static struct mem_block *g_head = NULL; /*!< Start (head) of our linked list */

static unsigned long g_allocations = 0; /*!< Allocation counter */

pthread_mutex_t alloc_mutex = PTHREAD_MUTEX_INITIALIZER; /*< Mutex for protecting the linked list */
/**
 * Function to search for the the first open block
 *
 * @param size: The size of block needed including allignment and struct 
 * @return: memory block pointer with size or NULL is nothing is open
 *
 */
void *first_fit(size_t size)
{
 
    struct mem_block *current = g_head;
    while(current != NULL){
        /* Check for first fit */
         if((current->size - current->usage)>= size ){
                /*return that block*/
                 return current;
        }
        /*Run through the nodes*/

            current = current->next; 
    }
    
    return NULL;
}
/**
 * Helper function that searches for a block with that size
 * After seraching creates a new block and return pointer back.
 *
 * @param size: The size of block needed including allignment and struct 
 * @return: memory block pointer with size or NULL is nothing is open
 *
 */
void *reuse(size_t size)
{
        if(g_head == NULL){
      /* First allcoation */
            return NULL;
        }

    /* Check which Algo to use */
    char *algo = getenv("ALLOCATOR_ALGORITHM");
    
    /* Default case*/
    if (algo == NULL) {
        algo = "first_fit";
    }

    void *to_return = NULL;
    /* redirection */
    if (strcmp(algo, "first_fit") == 0) {

        to_return  = first_fit(size);
     }else if (strcmp(algo, "best_fit") == 0) {
        to_return  = best_fit(size);
     }else if (strcmp(algo, "worst_fit") == 0) {
        to_return  = worst_fit(size);
     } 

    /* Returning pointer if not NULL */
    /* Adding data and returning pointer*/
    if(to_return != NULL){
        /* The block the pointer is currently at */
       struct mem_block *block = (struct mem_block *)to_return; 
        /*case that no splitting is needed if the block has no usage */
        /*update the already allocted struct */
        if(block->usage == 0){
            /*update block usage and return pointer */
            block->usage = size;
            return block + 1;
        }

        LOGP("Splitting block\n");
       /*Moving pointer to next open byte*/
        to_return += block->usage; 
        struct mem_block* new_block = (struct mem_block *) to_return;
        /* Adding info to new block */ 
        new_block->alloc_id = g_allocations++;
        new_block->size = block->size - block->usage;
        new_block->usage = size;
        new_block->region_start = block->region_start;
        /*Update linked list */
        /*Next block will be null new_block is last block */    
        new_block->next = block->next;
        /*Previous blocks next is new_block */
        block -> next = new_block;
        /*Update block usage size to the usage so it become a seperate block*/
        block->size = block->usage; 
        return new_block + 1;
    }
    /* Cannot reuse space */
    LOGP("Cannot reuse space\n");
    return NULL;
}
/**
 * Helper function that searches for biggest block that could hold that size.
 *
 * @param size: The size of block needed including allignment and struct 
 * @return: memory block pointer with size or NULL is nothing is open
 *
 */
void *worst_fit(size_t size)
{
    struct mem_block *current = g_head;
    struct mem_block *worst_fit = NULL;
    size_t current_space =0;
    size_t worst_space =0;
        while(current != NULL){
                current_space = (current->size) - (current->usage); 
         if(current_space >= size){
            /*check if the current worst fit is smaller - if so replace the blocks*/ 
            if(current_space>worst_space){
                /*update worst block*/
                worst_fit = current;
                /*update worst space */
                worst_space = current_space;
            }
        }
        /*Run through the nodes*/
        current = current->next;
        }
        return worst_fit;
    
}
/**
 * Helper function that searches for smallest block that could hold that size.
 *
 * @param size: The size of block needed including allignment and struct 
 * @return: memory block pointer with size or NULL is nothing is open
 *
 */
void *best_fit(size_t size)
{
    
    struct mem_block *current = g_head;
    struct mem_block *best_fit = NULL;
    size_t current_space =0;
    size_t best_space =SIZE_MAX;
     while(current != NULL){
        /* Check for first fit */
        current_space = (current->size) - (current->usage); 
         if(current_space >= size){
            /*check if the current best fit is smaller - if so replace the blocks*/ 
            if(current_space<best_space){
                /*update best  block*/
                best_fit = current;
                /*update best spcae */
                best_space= current_space;
            }
        }
        /*Run through the nodes*/
        current = current->next;
        }
    
    
       return best_fit;

}

/**
 * Helper function that allowes users to add names to the allocations.
 * Malloc does all the allocation and this funtion just adds name to the block
 * @param size: The size of block needed 
 * @param name: The name to add to the allocation 
 * @return: memory block pointer with size.
 *
 */
void *malloc_name(size_t size, char *name){
    LOG("Name: %s\n",name);
    void*to_return = malloc(size); 
    /*error*/

    pthread_mutex_lock(&alloc_mutex);
    if(to_return == NULL){

        return to_return;
    }
    /*Get struct of block returning */
    struct mem_block *block = (struct mem_block *)to_return -1;
    for (int i = 0; i < strlen(name) + 1; i++) 
    {
        block->name[i] = name[i];
    }

    pthread_mutex_unlock(&alloc_mutex);
    return block + 1;
    
}
/**
 * Malloc function
 * Function maps mem and checks to see if blocks can be resued. The is the main 
 * mapping function. In adittion this function also takes care of scriblling
 *
 * @param size: The size of block needed including allignment and struct 
 * @return: memory block pointer with size.
 *
 */
void *malloc(size_t size)
{
            pthread_mutex_lock(&alloc_mutex);
               char *scribble = getenv("ALLOCATOR_SCRIBBLE"); 
                size_t usage  = size  + sizeof (struct mem_block);
            
                /* Realign the memory request to 8 bytes */
                 if((usage % 8) != 0){
                        usage = usage + (8- usage % 8);
                        LOG("Alligned Size: %zu\n",usage); 
                }


                void *ptr = reuse(usage);

                if(ptr != NULL){
                    //LOG("Allocation number: %zu , pointer: %p\n",g_allocations, (ptr+1));
                     if(scribble != NULL){
                         if(atoi(scribble) == 1){
                            LOGP("Scribbiling\n");
                              memset(ptr,0XAA,size);
                        }
                    }
                    pthread_mutex_unlock(&alloc_mutex);
                    return ptr ;
                }

               
                int page_sz = getpagesize();
                size_t num_pages = usage/page_sz;
                if((usage % page_sz) != 0){
                        num_pages++;
                }

                size_t region_sz = num_pages * page_sz;
                LOG("Creating New Region: %zu bytes (%zu pages)\n",
                                region_sz, num_pages);

                struct mem_block *block = mmap( NULL, /* Adress (NULL for kernal to decide*/
                                region_sz, /* Size of memory block to allocate */
                                PROT_READ | PROT_WRITE, /* Memory protection flag */
                                MAP_PRIVATE | MAP_ANONYMOUS, /* Type of mapping */
                                -1, /* File descriptor */
                                0   /* Offset to start at within the mmaped memory */);

        if(block == MAP_FAILED){
                perror("mmap");
                return NULL;
        }

        block->alloc_id = g_allocations++;
        block->region_size = region_sz;
        block->size = region_sz;
        block->usage = usage;
        block->region_start = block;
        block->next = NULL;
    
    if(g_head == NULL){
        /*This was the first allocation */
        g_head = block;
    }else {
        struct mem_block *tail = g_head;
        while(tail->next != NULL){
            tail = tail->next;
        }
        tail->next = block;
    }
    //LOG("Allocation number: %zu , pointer: %p\n",g_allocations, (block+1));
     if(scribble != NULL){
        if(atoi(scribble) == 1){
         LOGP("Scribbiling\n");
         memset(block+1,0XAA,usage);
        }
    }
    pthread_mutex_unlock(&alloc_mutex);
     return block + 1;
}

/**
 * Main munmaping function. Checks if regions are empty and unmaps accordingly else
 * sets the block which is wanting to get freed to be empty usage.
 *
 * @param ptr: the location to which to what is asking to be free. 
 * @return: void 
 *
 */

void free(void *ptr)
{
    
    if (ptr == NULL) { 
        return;
    }
    LOG("Free request @ %p\n", ptr);
    /*Get the struct for updating data*/
    struct mem_block *block = (struct mem_block *) ptr -1;
    /*Usage becomes 0 */
    block->usage = 0;
    /*next block to connect*/
    struct mem_block *block_prev = g_head;
    struct mem_block *block_next = g_head;

    /*If the block is start if the linked list */

    if(block->region_start == g_head){
    /*Unmap the whole region becuase it is not being used after the one being freeed */
        if(block->next == NULL&& block == g_head){
        /*unmap mem the whole region*/
            int ret = munmap(block,block->region_size);
            if(ret == -1){
                perror("munmap");
            }
        /*NULL so malloc can make a new first node next allocation */ 
            g_head =NULL;
            LOGP("Unmapped the first node, finished freeing\n");
            //print_memory();
            return;
}
/*Remove the block and set head to block_next*/ 

        while(block_next != NULL && block_next->region_start == g_head){
            /*still being used */
            if(block_next->usage !=0){
                LOGP("CANNOT FREE REGION IN USE\n");
                return;
            }
            block_next = block_next->next;
        }
        int ret = munmap(g_head,g_head->region_size);
        if(ret == -1){
            perror("munmap");
        }
        g_head = block_next;
        return;
    }  


    /*Remove the whole region*/
    /*get head and tail of region */
    while(block_prev->next != block->region_start && block_prev->next != NULL){
        block_prev = block_prev->next;
        block_next = block_prev->next;
    }
    while(block_next == block->region_start && block_prev->next != NULL){
        block_next = block_next->next;
    }
    struct mem_block * current = block->region_start; 
    if(block->region_start == block){
        if(block->next == NULL){
            /*remove the region */
            int ret = munmap(block,block->region_size);
            if(ret == -1){
                perror("munmap");
            }
            /*connect the tail of previous region and head of next region */
            block_prev -> next = block_next;
            return;
    }

        while(current->region_start ==  block->region_start && current->next != NULL){
            /*cannot free region*/
            if(current->usage !=0){
                LOGP("Head but still in use\n");
                return;
            }
            current = current->next;
        }

        int ret = munmap(block,block->region_size);
        if(ret == -1){
             perror("munmap");
        }
        /*connect the tail of previous region and head of next region */
        block_prev -> next = block_next;
        return;


    }else {
        current = block->region_start;
        while(current->region_start ==  block->region_start && current->next != NULL){
            /*cannot free region*/
            if(current->usage !=0){
                LOGP("still in use\n");
                return;
            }
            current =  current ->next;

        }
        int ret = munmap(block,block->region_size);
        if(ret == -1){
            perror("munmap");
        }
        /*connect the tail of previous region and head of next region */
        block_prev -> next = block_next;
        return;
    }

    return;
}

/**
 * Calls Malloc but sets all mem points to zero. all mem mapping in done in malloc.
 * 
 * @param size: The size of block to allocate 
 * @param nmemb: Elements to allocate for 
 * @return: pointer to memory location
 *
 */

void *calloc(size_t nmemb, size_t size)
{
    void *ptr = malloc(nmemb* size);
    memset(ptr,0,nmemb *size);
    return ptr;
}

/**
 * This function trys to change the allocation location by finding the 
 * amount of size needed. 
 * @param size: The size of block to allocate 
 * @param ptr:  The space to reallocte 
 * @return: pointer to memory location of new allocation
 *
 */


void *realloc(void *ptr, size_t size)
{
    if (ptr == NULL) {
    /* If the pointer is NULL, then we simply malloc a new block */
        return malloc(size);
    }

    if (size == 0) {
    /* Realloc to 0 is often the same as freeing the memory block... But the
    * C standard doesn't require this. We will free the block and return
    * NULL here. */
        free(ptr);
        return NULL;
    }

    struct mem_block * block = (struct mem_block *)ptr -1;
    /*check if usage of the block is less than its size */
    if(block->usage < block->size){

        /* Means there is more space is block */
        size_t size_left = block->size - block->usage; 
        if(size_left>= size){
            LOGP("space is left\n");
            block->usage = size;
            return ptr;
        }
        /*malloc*/
        void * new_block= malloc(size);
        /*copy everything from block*/
        memcpy(new_block, ptr, block->usage);
        free(ptr);
        return new_block;
        }else{ 
            /*malloc*/
            void * new_block= malloc(size);
            /*copy everything from block*/
            memcpy(new_block, ptr, block->usage);
            free(ptr);
            return new_block;
        }

    return NULL;
}

/**
 * print_memory
 *
 * Prints out the current memory state, including both the regions and blocks.
 * Entries are printed in order, so there is an implied link from the topmost
 * entry to the next, and so on.
 */
void print_memory(void)
{
    puts("-- Current Memory State --");
    struct mem_block *current_block = g_head;
    struct mem_block *current_region = NULL;
    while (current_block != NULL) {
        if (current_block->region_start != current_region) {
            current_region = current_block->region_start;
            printf("[REGION] %p-%p %zu\n",
                    current_region,
                    (void *) current_region + current_region->region_size,
                    current_region->region_size);
        }
        printf("[BLOCK]  %p-%p (%lu) '%s' %zu %zu %zu\n",
                current_block,
                (void *) current_block + current_block->size,
                current_block->alloc_id,
                current_block->name,
                current_block->size,
                current_block->usage,
                current_block->usage == 0
                    ? 0 : current_block->usage - sizeof(struct mem_block));
        current_block = current_block->next;
    }
}

