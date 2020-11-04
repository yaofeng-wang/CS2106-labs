/*************************************
* Lab 4
* Name:
* Student No:
* Lab Group:
*************************************/

#include "shmheap.h"

/*
Additional helper private functions.
*/
int smhheap_get_prev_divider(shmheap_memory_handle mem, int offset);
int shmheap_convert_to_byte_aligned_size(int sz);

int SHMHEAP_BYTE_ALIGNMENT = 8;

shmheap_memory_handle shmheap_create(const char *name, size_t len) {
    // printf("In shmheap_create.....\n");

    //create new shared memory object via shm_open
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

    // store name, len, ptr in shmheap_memory_handle
    // name is used to uniquely identify the shared memory object
    shmheap_memory_handle handle;
    handle.name = (char *) name;
    handle.len = len;
    handle.ptr = ptr;


    // create first divider in shared heap
    sem_t *first_divider_ptr = (sem_t *) ptr;
    if (sem_init(first_divider_ptr, 1 , 1) != 0) {
        perror("sem_init failed\n");
        exit(1);
    }

    // create divider in shared heap
    shmheap_divider *divider_ptr =  (shmheap_divider *) (ptr + sizeof(sem_t));
    divider_ptr->next = len;
    divider_ptr->is_free = 1;

    // return shmheap_memory_handle
    return handle;
    
    // printf("Ended shmheap_create.....\n");
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
        perror("stat failed\n");
        exit(1);
    }
    int len = s.st_size;

    // create mapping between shared memory object and 
    // virtual address space via mmap 
    // returns ptr to address space
	void *ptr = mmap(NULL, len, PROT_WRITE | PROT_READ, MAP_SHARED_VALIDATE, fd, 0);
    if (ptr == MAP_FAILED) {
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

    // printf("Ended shmheap_connect\n");
    // return shmheap_memory_handle
    return handle;
}

void shmheap_disconnect(shmheap_memory_handle mem) {

    // unmap from shared memory object via munmap
	if (munmap(mem.ptr, mem.len) != 0) {
		perror("munmap failed\n");
		exit(1);
	}

    // // close semaphore
    // if (sem_close(mem.sem_ptr) != 0) {
    //     perror("sem_close failed\n");
    //     if (errno == EINVAL) {
    //         perror("invalid semaphore\n");
    //     }
    //     exit(1);
    // }
}

void shmheap_destroy(const char *name, shmheap_memory_handle mem) {

    // destroy semaphore
    if (sem_destroy(mem.ptr) != 0) {
        perror("sem_destroy failed\n");
        exit(1);
    }

	// unmap from shared memory object via munmap
	if (munmap(mem.ptr, mem.len) != 0) {
    	perror("munmap failed\n");
    	exit(1);
    }

	// unlink the shared memory via shm_unlink
    if (shm_unlink(name) == -1) {
    	perror("shm_unlink failed\n");
    	exit(1);
    }

    // // close semaphore
    // if (sem_close(mem.sem_ptr) != 0) {
    //     perror("sem_close failed\n");
    //     if (errno == EINVAL) {
    //         perror("invalid semaphore\n");
    //     }
    //     exit(1);
    // }

    // // removes named semaphore
    // if (sem_unlink(mem.name) != 0) {
    //     perror("sem_unlink failed\n");
    //     exit(1);
    // }
}

void *shmheap_underlying(shmheap_memory_handle mem) {

    return mem.ptr;
}

void *shmheap_alloc(shmheap_memory_handle mem, size_t sz) {
    // printf("In alloc.....\n");
    
    int cur;
    int capacity;
    sz = (size_t) shmheap_convert_to_byte_aligned_size((int) sz);
    shmheap_divider *divider;
 

    if (sem_wait(mem.ptr) != 0) {
        perror("sem_wait failed\n");
        exit(1);
    }

    cur = sizeof(sem_t);

    while (cur < (int) mem.len) {
        // printf("Total memory size: %ld\n", mem.len);
        // printf("Looking at: %d\n", cur);

        // look at the current divider.
        divider = (shmheap_divider *) (mem.ptr + cur);

        // check if the space on the right of the divider is free.
        if (divider->is_free == 1) {
            
            // Since the space is free,
            // calculate the capacity of this space.
            capacity = divider->next - cur - sizeof(shmheap_divider);

            // if capacity == sz
            // in this case, we don't need to add new divider
            // we need to :
            // (1) change is_free in divider to 0
            // (2) shift cur to position of data
            if (capacity == (int) sz) {
                // printf("1\n");
                // (1) change is_free in divider to 0
                divider->is_free = 0;

                // (2) shift cur to position of data
                cur += sizeof(shmheap_divider);

                break;
            }

            // if capacity >= size of data + new divider
            // in this case we need to allocate space for data and new divider
            // we need to:
            // (1) create new divider and insert it at the end of the new data
            // (2) update current divider
            // (3) shift cur to position of data
            else if (capacity >= (int) sz + (int) sizeof(shmheap_divider)) {
                // printf("2\n");

                // (1) create new divider and insert it at the end of the data
                shmheap_divider new_divider;
                new_divider.next = divider->next;
                new_divider.is_free = 1;
                shmheap_divider *new_divider_ptr =  (shmheap_divider *) (mem.ptr + cur + sizeof(shmheap_divider) + sz);
                *new_divider_ptr = new_divider;

                // (2) update current divider
                divider->is_free = 0;
                divider->next = cur + sizeof(shmheap_divider) + sz;

                // (3) shift cur to position of data
                cur += sizeof(shmheap_divider);

                break;
            }
        }
        cur = divider->next;
        // printf("%d\n", cur);
        
    }
 
    if (sem_post(mem.ptr) != 0) {
        perror("sem_post failed\n");
        exit(1);
    }

    // printf("cur: %d\n", cur);
    return mem.ptr + cur;
}

void shmheap_free(shmheap_memory_handle mem, void *ptr) {
    
    // printf("In free.....\n");

    if (sem_wait(mem.ptr) != 0) {
        perror("sem_wait failed\n");
        exit(1);
    }

    // get current divider
    int cur = ptr - mem.ptr - sizeof(shmheap_divider);
    shmheap_divider *divider = (shmheap_divider *) (mem.ptr + cur);

    // check if next divider exists 
    shmheap_divider *next_divider = NULL;
    if (divider->next != (int) mem.len) {
        next_divider = (shmheap_divider *) (mem.ptr + divider->next);
    }

    // check if previous divider exists 
    shmheap_divider *prev_divider = NULL;
    int prev = smhheap_get_prev_divider(mem, cur);
    if (prev != -1) {
        prev_divider = (shmheap_divider *) (mem.ptr + prev);
    }
    // if previous and next dividers both exist and are free
    // (1) update previous divider
    if (prev_divider != NULL && next_divider != NULL &&
            next_divider->is_free == 1 && prev_divider->is_free == 1) {
        
        // (1) update previous divider prev
        prev_divider->next = next_divider->next;

    }
    // if previous divider exists and is free (implies that next divider does not exist or is not free)
    // (1) update previous divider
    else if (prev_divider != NULL && prev_divider->is_free == 1) {

        // (1) update previous divider
        prev_divider->next = divider->next;

    }
    // if next divider exists and is free (implies that prev divider does not exist or is not free))
    // (1) update current divider
    else if (next_divider != NULL && next_divider->is_free == 1) {

        // (1) update current divider
        divider->next = next_divider->next;
        divider->is_free = 1;

    }
    // if previous and next dividers either does not exist or is not free
    // (1) update current divider
    else if ((next_divider == NULL || next_divider->is_free == 0) && (prev_divider == NULL || prev_divider->is_free == 0)) {
        
        // (1) update current divider
        divider->is_free = 1;
    }

    else {
        perror("Unknown condition in shmheap_free\n");
        printf("next divider exists: %d\n", next_divider != NULL);
        if (next_divider != NULL) {
            printf("next divider is free: %d\n", next_divider-> is_free);
        }
        printf("prev divider exists: %d\n", prev_divider != NULL);
        if (prev_divider != NULL) {
            printf("prev divider is free: %d\n", prev_divider-> is_free);
        }
        exit(1);
    }


    if (sem_post(mem.ptr) != 0) {
        perror("sem_post failed\n");
        exit(1);
    }
    // printf("Ended free.....\n");
}

shmheap_object_handle shmheap_ptr_to_handle(shmheap_memory_handle mem, void *ptr) {
    // store the name of the shared memory object
    char *name = mem.name;

    shmheap_object_handle handle;
    handle.name = name;
    handle.offset = ptr - mem.ptr;

    return handle;
}

void *shmheap_handle_to_ptr(shmheap_memory_handle mem, shmheap_object_handle hdl) {

	char *ptr = mem.ptr;
    size_t offset = hdl.offset;
    // printf("Offset: %ld\n", offset);
    return ptr + offset;
}

/*
    Gets the offset for the previous divider, if it exists.
    If previous divider does not exists, return -1.
*/
int smhheap_get_prev_divider(shmheap_memory_handle mem, int offset) {
    // printf("offset: %d\n", offset);
    int prev = -1;
    int cur = sizeof(sem_t);
    while (cur < offset) {
        shmheap_divider *ptr = (shmheap_divider *) (mem.ptr + cur);
        prev = cur;

        // printf("%d\n", cur);
        // if (ptr->next == 0) {
        //     printf("0!\n");
        //     printf("cur: %d\n", cur);
        //     exit(1);
        // }
        cur = ptr->next;
    }
    // printf("prev: %d\n", prev);
    return prev;
}


int shmheap_convert_to_byte_aligned_size(int sz) {
    return (sz % 8) + sz;
}

