/*************************************
* Lab 4
* Name:
* Student No:
* Lab Group:
*************************************/

#include "shmheap.h"

typedef struct {
    // position of next divider relative to the start of heap, 
    // value = length of heap if next divider does not exist.
    int next;
    // position of previous divider relative to the start of the heap, 
    // -1 if prev divider does not exist.
    int prev;
    // Whether the data to the right is free    
    int is_free; 
} shmheap_divider;


shmheap_memory_handle shmheap_create(const char *name, size_t len) {

    // create new shared memory object via shm_open
    int fd = shm_open(name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		perror("open failed\n");
		exit(1);
	}

	// resize shared memory object to a specific length via ftruncate
	if (ftruncate(fd, len) != 0) {
		perror("ftruncate failed\n");
		exit(1);
	}

	// create mapping between shared memory object and 
    // virtual address space via mmap 
    // returns ptr to address space
	void *ptr = mmap(NULL, len, PROT_WRITE | PROT_READ, MAP_SHARED_VALIDATE,fd, 0);
    if (ptr == (void *) -1) {
    	perror("mmap failed\n");
    	exit(1);
    }

    // close fd
    if (close(fd) == -1) {
        perror("close failed\n");
        exit(1);
    }

    // store name, len, ptr in shmheap_memory_handle
    // name is used to uniquely identify the shared memory object
    shmheap_memory_handle handle;
    handle.name = (char *) name;
    handle.len = len;
    handle.ptr = ptr;

    // create first divider in heap
    ptr = (shmheap_divider *) ptr;
    shmheap_divider divider;
    divider.next = len;
    divider.prev = -1;
    divider.is_free = 1;
    *ptr = divider;

    // return shmheap_memory_handle
    return handle;

}

shmheap_memory_handle shmheap_connect(const char *name) {

    // open shared memory object that already exists via shm_open
	int fd = shm_open(name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		perror("open failed\n");
		exit(1);
	}

    // get size of shared memory
    struct stat s;
    if(fstat(fd, &s) != 0) {
        perror("stat failed");
        exit(1);
    }
    int len = s.st_size;

    // create mapping between shared memory object and 
    // virtual address space via mmap 
    // returns ptr to address space
	void *ptr = mmap(NULL, len, PROT_WRITE | PROT_READ, MAP_SHARED_VALIDATE, fd, 0);
    if (ptr == (void *) -1) {
    	perror("mmap failed\n");
    	exit(1);
    }

    // close fd
    if (close(fd) == -1) {
        perror("close failed\n");
        exit(1);
    }

    // store name, len in shmheap_memory_handle
    // name is used to uniquely identify the shared memory object
    shmheap_memory_handle handle;
    handle.name = (char *) name;
    handle.len = len;
    handle.ptr = ptr;


    // attempt to read first divider in heap
    shmheap_divider *divider = (shmheap_divider *) ptr;
    printf("1st divider:\n");
    printf("next: %d\n", divider->next);
    printf("prev: %d\n", divider->prev);
    printf("is_free: %d\n", divider->is_free);

    // return shmheap_memory_handle
    return handle;
}

void shmheap_disconnect(shmheap_memory_handle mem) {

    // unmap from shared memory object via munmap
	if (munmap(mem.ptr, mem.len) != 0) {
		perror("munmap failed\n");
		exit(1);
	}
}

void shmheap_destroy(const char *name, shmheap_memory_handle mem) {

	// unmap from shared memory object via munmap
	if (munmap(mem.ptr, mem.len) != 0) {
    	perror("munmap failed\n");
    	exit(1);
    }

	// unlink the shared memory via shm_unlink
    if (shm_unlink(name) == -1) {
    	perror("unlink failed\n");
    	exit(1);
    }
}

void *shmheap_underlying(shmheap_memory_handle mem) {
    (void) mem;
    void * ptr;
    return ptr;
}

void *shmheap_alloc(shmheap_memory_handle mem, size_t sz) {

    int cur = 0;
    int capacity;

    // while (cur < mem.len) {

    //     // look at the current divider.
    //     shmheap_divider *divider = (shmheap_divider *) (mem.ptr + cur);

    //     // check if the space on the right of the divider is free.
    //     if (divider->is_free == 1) {

    //         // Since the space is free,
    //         // calculate the capacity of this space.
    //         capacity = divider->next - cur - sizeof(shmheap_divider);

    //         // if capacity == sz,
    //         // we need to :
    //         // (1) change is_free in divider to 0
    //         // (2) shift cur to position of data
    //         if (capacity == sz) {

    //             // (1) change is_free in divider to 0
    //             divider->if_free = 0;

    //             // (2) shift cur to position of data
    //             cur += sizeof(shmheap_divider);
    //             break;
    //         }

    //         // if capacity is able to accomodate new object + new divider
    //         // we need to:
    //         // (1) create new divider and insert it at the end of the data
    //         // (2) update current divider
    //         // (3) update divider after current divider
    //         // (4) shift cur to position of data
    //         else if (capacity >= size + sizeof(shmheap_divider)) {

    //             // (1) create new divider and insert it at the end of the data
    //             shmheap_divider new_divider;
    //             new_divider.next = divider.next;
    //             new_divider.prev = cur;
    //             new_divider.is_free = (int) ((capacity- sizeof(shmheap_divider) - sz) > 0);
    //             *(mem.ptr + cur + sizeof(shmheap_divider) + sz) = new_divider;

    //             // (2) update current divider
    //             divider.is_free = 0;
    //             divider.next = cur + sizeof(shmheap_bk) + sz;

    //             // (3) update divider after current divider
    //             if (new_divider.next == mem.len) {
    //                 // don't need to update next divider 
    //                 // because it does not exist
    //             } else {
    //                 // divider after current divider exists
    //                 shmheap_divider *next_divider = (shmheap_divider *) (mem.ptr + new_divider.next);
    //                 next_divider.prev = cur + sizeof(shmheap_divider)+ sz;
    //             }

    //             // (4) shift cur to position of data
    //             cur += sizeof(shmheap_divider);
    //             break;
    //         }
    //     }
    //     cur = divider.next;
    // }
    return mem.ptr + cur;
}

void shmheap_free(shmheap_memory_handle mem, void *ptr) {
    (void) mem;
    (void) ptr;
    // go to current divider
    // int cur = ptr - mem.ptr - sizeof(shmheap_divider);
    // shmheap_divider *divider = (shmheap_divider *) (mem.ptr + cur);
    // shmheap_divider *next_divider = (shmheap_divider *) (mem.ptr + divider.next);
    // shmheap_divider *prev_divider = (shmheap_divider *) (mem.ptr + divider.prev);

    // // if previous and next dividers are free
    // // (1) update previous divider
    // // (2) update divider after next divider
    // // (3) remove current divider
    // if (next_divider->is_free == 1 && prev_divider->is_free == 1) {
    //     shmheap_divider *nnext_divider = (shmheap_divider *) (mem.ptr + next_divider.next);

    //     nnext_divider.

    //     pprev_divider.next = divider.next;
    //     next_divider.prev = divider.prev;

    //     *divider = NULL;
    // }
    // // if previous divider is free
    // // then remove current divider
    // // update previous divider
    // // update next divider
    // else if (prev_divider->is_free == 1) {
    // }
    // // if next divider is free
    // // then remove current divider
    // // update previous divider
    // // update next divider
    // else if (next_divider->is_free == 1) {

    // }
    // // if previous and next dividers are both not free
    // // update current divider
    // else {

    // }
}

shmheap_object_handle shmheap_ptr_to_handle(shmheap_memory_handle mem, void *ptr) {
    (void) ptr;

    // store the name of the shared memory object
    char *name = mem.name;

    shmheap_object_handle handle;
    handle.name = name;
    handle.offset = 0;

    return handle;
}

void *shmheap_handle_to_ptr(shmheap_memory_handle mem, shmheap_object_handle hdl) {

	char *ptr = mem.ptr;
    size_t offset = hdl.offset;

    return ptr + offset;
}
