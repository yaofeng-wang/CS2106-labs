/*************************************
* Lab 4
* Name:
* Student No:
* Lab Group:
*************************************/

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

/*
You should modify these structs to suit your implementation,
but remember that all the functions declared here must have
a signature that is callable using the APIs specified in the
lab document.

You may define other helper structs or convert the existing
structs to typedefs, as long as the functions satisfy the
requirements in the lab document.  If you declare additional names (helper structs or helper functions), they should be prefixed with "shmheap_" to avoid potential name clashes.
*/


typedef struct {
    // position of next divider relative to the start of heap, 
    // value = length of heap if next divider does not exist.
    int next;

    // Whether the data to the right is free    
    int is_free; 
} shmheap_divider; // 8 bytes



typedef struct {
    // position of next divider relative to the start of heap, 
    // value = length of heap if next divider does not exist.
    int next;

    // Whether the data to the right is free    
    int is_free; 

    // 
    sem_t *sem_ptr;

    // offset to the additional shared memory
    int next_mem;

    

} shmheap_first_divider; // 8 + 32 + 4


typedef struct {
	char *name;
	void *ptr;
	size_t len;

	sem_t *sem_ptr;
} shmheap_memory_handle;


typedef struct {
	char *name;
	size_t offset;
} shmheap_object_handle;



/*
These functions form the public API of your shmheap library.
*/

shmheap_memory_handle shmheap_create(const char *name, size_t len);
shmheap_memory_handle shmheap_connect(const char *name);
void shmheap_disconnect(shmheap_memory_handle mem);
void shmheap_destroy(const char *name, shmheap_memory_handle mem);
void *shmheap_underlying(shmheap_memory_handle mem);
void *shmheap_alloc(shmheap_memory_handle mem, size_t sz);
void shmheap_free(shmheap_memory_handle mem, void *ptr);
shmheap_object_handle shmheap_ptr_to_handle(shmheap_memory_handle mem, void *ptr);
void *shmheap_handle_to_ptr(shmheap_memory_handle mem, shmheap_object_handle hdl);



