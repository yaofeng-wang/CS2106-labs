#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "zc_io.h"

// The zc_file struct is analogous to the FILE struct that you get from fopen.
struct zc_file {
  // pointer to the virtual memory space
  void *ptr;
  // offset from the start of the virtual memory
  long offset;
  // total size of the file
  long size;
  // file descriptor to the opened file
  int fd;
  // mutex for access to the memory space
  pthread_mutex_t *buffer_mutexes;
  // mutex for modifying the number of readers
  pthread_mutex_t num_readers_mutex;
  // number of readers
  int *num_readers;
};

// helper functions
void update_ptr_to_virtual_address(zc_file *file, int new_size);

/**************
 * Exercise 1 *
 **************/

zc_file *zc_open(const char *path) {

  // allocate space for zc_file
  zc_file *file_ptr = (zc_file *) malloc(sizeof(zc_file));
  if (file_ptr == NULL) {
    perror("malloc failed\n");
    return NULL;
  }

  // set offset to 0
  file_ptr->offset = 0;

  // open file using open to get file descriptor
  file_ptr->fd = open(path, O_CREAT | O_RDWR, S_IRWXU);
  if (file_ptr->fd == -1) {
    perror("open failed\n");
    return NULL;
  }

  // get size of file 
  struct stat statbuf;
  if (fstat(file_ptr->fd, &statbuf) != 0) {
    perror("fstat failed\n");
    return NULL;
  }
  int size = statbuf.st_size;

  // map file into virtual address space
  file_ptr->ptr = NULL;
  update_ptr_to_virtual_address(file_ptr, size);

  // initialise synchronization resources 
  int num_pages = (int) ceil(size / sysconf(_SC_PAGESIZE));
  pthread_mutex_init(&(file_ptr->num_readers_mutex), NULL);

  file_ptr->buffer_mutexes = malloc(sizeof(pthread_mutex_t) * num_pages);
  for (int i = 0; i < num_pages; i++) {
    pthread_mutex_init(&(file_ptr->buffer_mutexes[i]), NULL);
  }
  
  file_ptr->num_readers = malloc(sizeof(int) * num_pages);
  for (int i = 0; i < num_pages; i++) {
    file_ptr->num_readers[i] = 0;
  }
  
  return file_ptr;
}

int zc_close(zc_file *file) {
  
  // flush updates into file
  if (msync(file->ptr, file->size, MS_SYNC) != 0) {
    perror("mysnc failed\n");
    return -1;
  }

  // unmap file from memory
  if (munmap(file->ptr, file->size) != 0) {
      perror("munmap failed\n");
      return -1;
  }

  // close file descriptor
  if (close(file->fd) != 0) {
   perror("close failed\n");
   return -1;
  }
  // de-allocate heap memory
  free(file->buffer_mutexes);
  file->buffer_mutexes = NULL;
  free(file->num_readers);
  file->num_readers = NULL;
  free(file);
  file = NULL;

  return 0;
}

const char *zc_read_start(zc_file *file, size_t *size) {
  if (pthread_mutex_lock(&(file->num_readers_mutex)) != 0) {
    perror("pthread_mutex_lock failed\n");
    return NULL;    
  }
  if (file->num_readers == 0) {
    if (pthread_mutex_lock(&(file->buffer_mutexes[0])) != 0) {
      perror("pthread_mutex_lock failed\n");
      return NULL;    
    }
  }

  file->num_readers++;

  if (pthread_mutex_unlock(&(file->num_readers_mutex)) != 0) {
    perror("pthread_mutex_unlock failed\n");
    return NULL;    
  }


  // invalid offset
  if (file->offset < 0 || file->offset >= file->size) {
    *size = 0;
    return NULL;    
  }

  int old_offset = file->offset;
  int capacity = file->size - file->offset;
  
  // if size of file >= *size bytes remaining
  if (capacity >= (int) *size) {
  
    // update offset
    file->offset += *size;
  }
  else {
    // update value of *size
    *size = (size_t) capacity;

    // update offset
    file->offset += capacity; 
  }

  // return pointer
  return file->ptr + old_offset;  

}

void zc_read_end(zc_file *file) {
  if (pthread_mutex_lock(&(file->num_readers_mutex)) != 0) {
    perror("pthread_mutex_lock failed\n");
    exit(1);
  }

  file->num_readers--;

  if (file->num_readers == 0) {
    if (pthread_mutex_unlock(&(file->buffer_mutexes[0])) != 0) {
      perror("pthread_mutex_unlock failed\n");
      exit(1);
    }
  }

  if (pthread_mutex_unlock(&(file->num_readers_mutex)) != 0) {
    perror("pthread_mutex_unlock failed\n");
    exit(1);
  }
  
}

/**************
 * Exercise 2 *
 **************/

char *zc_write_start(zc_file *file, size_t size) {

  if (pthread_mutex_lock(&(file->buffer_mutexes[0])) != 0) {
    perror("pthread_mutex_lock failed\n");
    return NULL;
  }

  // invalid offset
  if (file->offset < 0) {
    return NULL;
  }

  // check if offset is beyond size of file
  // if it is, fill gap with '\0' characters
  if (file->offset > file->size) {
    int old_size = file->size;

    // increase size of file
    if (ftruncate(file->fd, file->offset) != 0) {
      perror("ftruncate failed\n");
      return NULL;
    }

    update_ptr_to_virtual_address(file, file->offset);
    memset(file->ptr+old_size, 0, file->offset-old_size);

  }
  
  int old_offset = file->offset;
  int capacity = file->size - file->offset;
  

  // if file not mapped to virtual address yet OR
  // if size of mapped memory < size, we need to:
  // (1) increase size of file
  // (2) update mapping in virtual memory
  if (file->ptr == NULL || capacity < (int) size) {
    // update size
    int new_size = file->size + size - capacity;

    // increase size of file
    if (ftruncate(file->fd, new_size) != 0) {
      perror("ftruncate failed\n");
      return NULL;
    }

    update_ptr_to_virtual_address(file, new_size);
  }

  // update offset
  file->offset += size;

  // return pointer to original offset
  return file->ptr + old_offset;

}

void zc_write_end(zc_file *file) {

  // flush updates into file
  if (msync(file->ptr, file->size, MS_SYNC) != 0) {
    perror("mysnc failed\n");
    exit(1);
  }

  if (pthread_mutex_unlock(&(file->buffer_mutexes[0])) != 0) {
    perror("pthread_mutex_unlock failed\n");
    exit(1);
  }

}

/**************
 * Exercise 3 *
 **************/

off_t zc_lseek(zc_file *file, long offset, int whence) {

  if (pthread_mutex_lock(&(file->buffer_mutexes[0])) != 0) {
    perror("pthread_mutex_lock failed\n");
    return (off_t) -1;
  }

  int retval;
  switch (whence) {
    case SEEK_SET:
      retval = (int) offset;
      break;
    case SEEK_CUR:
      retval = file->offset + (int) offset;
      break;
    case SEEK_END:
      retval = file->size + (int) offset;
      break;
    default:
      retval = (off_t) -1;
  }

  if (retval < 0) {
    return (off_t) -1;
  }

  if (retval != (off_t) - 1) {
    file->offset = retval;
  }

  if (pthread_mutex_unlock(&(file->buffer_mutexes[0])) != 0) {
    perror("pthread_mutex_unlock failed\n");
    return (off_t) -1;;
  }

  return retval;
}

/**************
 * Exercise 5 *
 **************/

int zc_copyfile(const char *source, const char *dest) {


  // check that the 2 file names are not NULL
  if (source == NULL || dest == NULL) {
    return -1;
  }

  // open source file
  zc_file *source_zc_file = zc_open(source);
  if (source_zc_file == NULL) {
    return -1;
  }

  // open dest file
  zc_file *dest_zc_file = zc_open(dest);
  if (dest_zc_file == NULL) {
    return -1;
  }

  // set read pointer to source file
  // and check that expected read size is equals to actual read size
  long read_size = source_zc_file->size;
  char *read_ptr = zc_read_start(source_zc_file, (size_t *) &read_size);
  if (read_ptr == NULL) {
    return -1;
  }

  if (read_size != source_zc_file->size) {
    return -1;
  }


  // re-size of dest file so that it has the same size as source file
  if (ftruncate(dest_zc_file->fd, source_zc_file->size) != 0) {
    perror("ftruncate failed\n");
    return -1;
  }
  update_ptr_to_virtual_address(dest_zc_file, source_zc_file->size);

  // get write pointer to dest file
  char *write_ptr = zc_write_start(dest_zc_file, source_zc_file->size);
  if (write_ptr == NULL) {
    return -1;
  }

  // copy content from source file to dest file
  memcpy(write_ptr, read_ptr, source_zc_file->size);

  // end read from from source file
  zc_read_end(source_zc_file);

  // end write from dest file
  zc_write_end(dest_zc_file);

  // close source file
  if (zc_close(source_zc_file) != 0) {
    perror("zc_close failed\n");
    return -1;
  }

  // close dest file
  if (zc_close(dest_zc_file) != 0) {
    perror("zc_close failed\n");
    return -1;
  }

  return 0;
}


void update_ptr_to_virtual_address(zc_file *file, int new_size) {
  // if new_size is 0, then don't map into virtual memory
  if (new_size == 0) {
    file->ptr = NULL;  
  }
  else if (file->ptr != NULL) {
    // remap memory
    file->ptr = mremap(file->ptr, file->size, new_size, MREMAP_MAYMOVE);
    if (file->ptr == MAP_FAILED) {
      perror("mremap failed\n");
      exit(1);
    }
  }
  else {
    // map memory
    file->ptr = mmap(NULL, new_size, PROT_READ | PROT_WRITE, MAP_SHARED_VALIDATE, file->fd, 0);
    if (file->ptr == MAP_FAILED) {
      perror("mmap failed\n");
      free(file);
      exit(1);
    }
  }
  file->size = new_size;
}
