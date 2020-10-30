/*************************************
* Lab 4
* Name:
* Student No:
* Lab Group:
*************************************/

#include "mmf.h"

void *mmf_create_or_open(const char *name, size_t sz) {
    // create file via open
	int fd = open(name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		perror("open failed\n");
		exit(1);
	}

	// resize file to a specific length via ftruncate
	if (ftruncate(fd, sz) == -1) {
		perror("ftruncate failed\n");
		exit(1);
	}

    // map file contents into memory via mmap
    void *ptr = mmap(NULL, sz, PROT_WRITE | PROT_READ, MAP_SHARED_VALIDATE, fd, 0);
    if (ptr == (void *) -1) {
    	perror("mmap failed\n");
    	exit(1);
    }
    
	// close file descriptor
    if (close(fd) == -1) {
    	perror("close failed\n");
    	exit(1);
    }

    // return pointer to the allocated page
    return ptr;
}

void mmf_close(void *ptr, size_t sz) {
    // delete mapping via munmap
    if (munmap(ptr, sz) == -1) {
    	perror("munmap failed\n");
    	exit(1);
    }
}
