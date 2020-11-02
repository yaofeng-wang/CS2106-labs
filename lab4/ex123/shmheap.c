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
void smhheap_update(shmheap_memory_handle mem);
int smhheap_get_prev_divider(shmheap_memory_handle mem, int offset);
int shmheap_convert_to_byte_aligned_size(int sz);


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

    // create named semaphore with the same name as shared memory
    // add pointer to semaphore to handler
    sem_t *sem_ptr = sem_open(name, O_CREAT, S_IRUSR | S_IWUSR, 1);
    if (sem_ptr == SEM_FAILED) {
        perror("sem_open failed\n");
        exit(1);
    }
    handle.sem_ptr = sem_ptr;

    // create first divider in shared heap
    shmheap_divider *divider_ptr =  (shmheap_divider *) ptr;
    divider_ptr->next = len;
    divider_ptr->is_free = 1;

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

    // open named semaphore with the same name as shared memory
    // add pointer to semaphore to handler
    sem_t *sem_ptr = sem_open(name, O_RDWR);
    if (sem_ptr == SEM_FAILED) {
        perror("sem_open failed\n");
        exit(1);
    }
    handle.sem_ptr = sem_ptr;

    // return shmheap_memory_handle
    return handle;
}

void shmheap_disconnect(shmheap_memory_handle mem) {

    // unmap from shared memory object via munmap
	if (munmap(mem.ptr, mem.len) != 0) {
		perror("munmap failed\n");
		exit(1);
	}

    // close semaphore
    if (sem_close(mem.sem_ptr) != 0) {
        perror("sem_close failed\n");
        if (errno == EINVAL) {
            perror("invalid semaphore\n");
        }
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
    	perror("shm_unlink failed\n");
    	exit(1);
    }

    // close semaphore
    if (sem_close(mem.sem_ptr) != 0) {
        perror("sem_close failed\n");
        if (errno == EINVAL) {
            perror("invalid semaphore\n");
        }
        exit(1);
    }

    // removes named semaphore
    if (sem_unlink(mem.name) != 0) {
        perror("sem_unlink failed\n");
        exit(1);
    }
}

void *shmheap_underlying(shmheap_memory_handle mem) {

    // update virtual address
    smhheap_update(mem);

    return mem.ptr;
}

void *shmheap_alloc(shmheap_memory_handle mem, size_t sz) {

    int cur = 0;
    int capacity;
    int insufficient_space = 1;
    int sz = shmheap_convert_to_byte_aligned_size((int) sz);

    if (sem_wait(mem.sem_ptr) != 0) {
        perror("sem_wait failed\n");
        exit(1);
    }

    // update virtual address
    smhheap_update(mem);

    while (cur < (int) mem.len) {

        // look at the current divider.
        shmheap_divider *divider = (shmheap_divider *) (mem.ptr + cur);

        // check if the space on the right of the divider is free.
        if (divider->is_free == 1) {

            // Since the space is free,
            // calculate the capacity of this space.
            capacity = divider->next - cur - sizeof(shmheap_divider);

            // if capacity == sz and there exists a next divider
            // in this case, we don't need to add new divider
            // we need to :
            // (1) change is_free in divider to 0
            // (2) shift cur to position of data
            // (3) update insufficient_space flag
            if (capacity == (int) sz && divider->next != mem.len) {

                // (1) change is_free in divider to 0
                divider->is_free = 0;

                // (2) shift cur to position of data
                cur += sizeof(shmheap_divider);

                // (3) update insufficient_space flag
                insufficient_space = 0;
                break;
            }

            // if capacity >= size of data + new divider
            // in this case we need to allocate space for data and new divider
            // we need to:
            // (1) create new divider and insert it at the end of the new data
            // (2) update current divider
            // (3) shift cur to position of data
            // (4) update insufficient_space flag
            else if (capacity >= (int) sz + (int) sizeof(shmheap_divider)) {

                // (1) create new divider and insert it at the end of the data
                shmheap_divider new_divider;
                new_divider.next = divider->next;
                new_divider.is_free = (int) ((capacity- sizeof(shmheap_divider) - sz) > 0);
                shmheap_divider *new_divider_ptr =  (shmheap_divider *) (mem.ptr + cur + sizeof(shmheap_divider) + sz);
                *new_divider_ptr = new_divider;

                // (2) update current divider
                divider->is_free = 0;
                divider->next = cur + sizeof(shmheap_divider) + sz;

                // (3) shift cur to position of data
                cur += sizeof(shmheap_divider);

                // (4) update insufficient_space flag
                insufficient_space = 0;
                break;
            }
        }
        cur = divider->next;
    }

    // none of the holes are able to accomodate the requested space
    // need to increase size of shared memory space
    if (insufficient_space) {

        // open shared memory space
        int fd = shm_open(mem.name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (fd == -1) {
            perror("open failed\n");
            exit(1);
        }

        // increase the size of the shared memory space 
        // by sz + size of 1 divider
        int new_len = mem.len + sz + sizeof(shmheap_divider);
        if (ftruncate(fd, new_len) != 0) {
            perror("ftruncate failed\n");
            exit(1);
        }

        // remap shared memory to virtual address space
        void *ptr = mremap(mem.ptr, mem.len, new_len, MREMAP_MAYMOVE);
        if (ptr == MAP_FAILED) {
            perror("mremap failed\n");
            exit(1);
        }

        // store shared memory information
        mem.len = new_len;
        mem.ptr = ptr;
    }

    if (sem_post(mem.sem_ptr) != 0) {
        perror("sem_post failed\n");
        exit(1);
    }

    return mem.ptr + cur;
}

void shmheap_free(shmheap_memory_handle mem, void *ptr) {

    if (sem_wait(mem.sem_ptr) != 0) {
        perror("sem_wait failed\n");
        exit(1);
    }

    // update virtual address
    smhheap_update(mem);

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
    int prev = smhheap_get_prev_divider(mem, offset);
    if (prev != -1) {
        prev_divider = (shmheap_divider *) (mem.ptr + divider->prev);
    }

    // if previous and next dividers both exist and are free
    // (1) update previous divider
    // (2) update divider after next divider (if it exists)
    // (3) remove current and next divider 
    if (prev_divider != NULL && next_divider != NULL &&
            next_divider->is_free == 1 && prev_divider->is_free == 1) {
        
        // (1) update previous dividerprev
        prev_divider->next = next_divider->next;

        // (2) update divider after next divider (if it exists)
        if (next_divider->next != (int) mem.len) {
            // nnext exists (and implies that it_is not free)
            shmheap_divider *nnext_divider = (shmheap_divider *) (mem.ptr + next_divider->next);
            nnext_divider->prev = divider->prev;
            assert(nnext_divider->is_free == 0);
        }
        
        // (3) remove current and next divider 
        // *divider = NULL;
        // *next_divider = NULL;
    }
    // if previous divider exists and is free (implies that next divider does not exist or is not free)
    // (1) update previous divider
    // (2) update next divider (if it exists)
    // (3) remove current divider
    else if (prev_divider != NULL && prev_divider->is_free == 1) {

        // (1) update previous divider
        prev_divider->next = divider->next;

        // (2) update next divider (if it exists)
        if (next_divider != NULL) {
            // next exists
            next_divider->prev = divider->prev;
        }
        // (3) remove current divider
        // *divider = NULL;
    }
    // if next divider exists and is free (implies that prev divider does not exist or is not free))
    // (1) update current divider
    // (2) remove next divider 
    else if (next_divider != NULL && next_divider->is_free == 1) {

        // (1) update current divider
        divider->next = next_divider->next;

        // (2) remove next divider 
        // *next_divider = NULL;
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
    if (sem_post(mem.sem_ptr) != 0) {
        perror("sem_post failed\n");
        exit(1);
    }
}

shmheap_object_handle shmheap_ptr_to_handle(shmheap_memory_handle mem, void *ptr) {
    (void) ptr;

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

    return ptr + offset;
}

void smhheap_update(shmheap_memory_handle mem) {

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
    int new_len = s.st_size;
    

    // if our recorded memory size is not the same the the current
    // size of the shared memory, it means that we have an
    // outdated version of shared memory and we should update our record
    if (mem_len != new_len) {
        
        // remap shared memory to virtual address space
        void *ptr = mremap(mem.ptr, mem.len, new_len, MREMAP_MAYMOVE);
        if (ptr == MAP_FAILED) {
            perror("mremap failed\n");
            exit(1);
        }

        // store shared memory information
        mem.len = new_len;
        mem.ptr = ptr;
    }
}

/*
    Gets the offset for the previous divider, if it exists.
    If previous divider does not exists, return -1.
*/
int smhheap_get_prev_divider(shmheap_memory_handle mem, int offset) {
    
    int prev = -1;
    int cur = 0;
    while (cur < offset) {
        shmheap_memory_handle *ptr = (shmheap_memory_handle *) (mem.ptr + cur);
        prev = cur;
        cur += ptr->next;
    }

    return prev;
}


int shmheap_convert_to_byte_aligned_size(int sz) {
    return sz % 8 + sz;
}

