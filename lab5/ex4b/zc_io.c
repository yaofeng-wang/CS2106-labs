#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "zc_io.h"

#define IF_TRUE_THEN_FAILED_TO_READ(cond, msg) do { if (cond) {perror(msg); *size = 0; return NULL;}} while(0)
#define IF_TRUE_THEN_EXIT_ONE(cond, msg) do {if (cond) {perror(msg); exit(1);}} while(0)
#define IF_TRUE_THEN_FAILED_TO_WRITE(cond, msg) do {if (cond) {perror(msg); return NULL;}} while(0)
#define IF_TRUE_THEN_FAILED_TO_LSEEK(cond, msg) do {if (cond) {perror(msg); return (off_t) -1;}} while(0)

typedef enum {INIT_COUNT, INCREASE_COUNT, DECREASE_COUNT} update_readers_mode;
typedef enum {READ, WRITE} mode;

typedef struct zc_access_info zc_access_info;
struct zc_access_info {
  // pointer to next entry
  zc_access_info *next;
  // thred ID
  pid_t thread_id;
  // starting index for the access
  int start_index;
  // ending index for the access
  int end_index;

};


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
  sem_t *buffer_mutexes;
  // mutex for modifying the number of readers
  sem_t try_to_access_buffer_mutex;
  // number of readers
  int *num_readers;
  // linked list containing access_info
  zc_access_info* head_ptr;

};

// helper functions
int update_ptr_to_virtual_address(zc_file *file, long new_size);
long get_file_size(zc_file *file);
int init_sync_resources(zc_file *file);
int get_index(long num);
int calc_num_pages(zc_file *file);
int add_access_info_entry(zc_file *file, int start_index, int end_index);
int update_num_readers(zc_file *file, int start_index, int end_index, update_readers_mode mode);
int unlock_mutexes(zc_file *file, int start_index, int end_index, int* locked_mutexes);
int wait_try_to_access_buffer_mutex(zc_file *file);
int try_to_get_mutexes(zc_file *file, int start_index, int end_index, int *locked_mutexes, int *able_to_get_mutexes, mode mode);
int post_try_to_access_buffer_mutex(zc_file *file);
void set_start_and_end_index(zc_file *file, size_t* size, int *start_index, int *end_index);
zc_access_info *remove_access_info_entry(zc_file *file);
int update_sync_resources(zc_file *file, long size);
int update_access_info_entry(zc_file *file, int end_index);
int update_file_size(zc_file *file, long new_size, int fill_with_null);

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
  long size = get_file_size(file_ptr);
  if (size == -1) {
    return NULL;
  }

  // map file into virtual address space
  file_ptr->ptr = NULL;
  if (update_ptr_to_virtual_address(file_ptr, size) == -1) {
    return NULL;
  }
  file_ptr->size = size;

  // initialise synchronization resources 
  if (init_sync_resources(file_ptr) == -1) {
    return NULL;
  }
  
  // initialise head_ptr
  file_ptr->head_ptr = NULL;

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

  // destroy semaphores
  int num_pages = calc_num_pages(file);
  for (int i = 0; i < num_pages; i++) {
    if(sem_destroy(&(file->buffer_mutexes[i])) != 0) {
      perror("sem_destroy failed\n");
      return -1;
    }
  }
  if (sem_destroy(&(file->try_to_access_buffer_mutex)) != 0) {
    perror("sem_destroy failed\n");
    return -1;
  }

  // de-allocate heap memory
  if (file->head_ptr) {
    zc_access_info *ptr = file->head_ptr;
    zc_access_info *next;
    while (ptr->next) {
      next = ptr->next;
      free(ptr);
      ptr = next;
    }
    free(file->head_ptr);
  } 

  if (file->buffer_mutexes) {
    free(file->buffer_mutexes);
    file->buffer_mutexes = NULL;
  }


  if (file->num_readers) {
    free(file->num_readers);
    file->num_readers = NULL;
  }


  free(file);
  file = NULL;

  return 0;
}

const char *zc_read_start(zc_file *file, size_t *size) {

  while (1) {

    IF_TRUE_THEN_FAILED_TO_READ(wait_try_to_access_buffer_mutex(file) != 0, 
      "wait_try_to_access_buffer_mutex failed\n");

    int num_pages = calc_num_pages(file);
    int *locked_mutexes = malloc(num_pages * sizeof(int));
    IF_TRUE_THEN_FAILED_TO_READ(locked_mutexes == NULL, "malloc failed\n");
    for (int i=0; i < num_pages; i++) {
      locked_mutexes[i] = 0;
    }
    int able_to_get_mutexes = 1;
    int start_index, end_index;
    set_start_and_end_index(file, size, &start_index, &end_index);

    // for each of the pages that we need to read
    IF_TRUE_THEN_FAILED_TO_READ(try_to_get_mutexes(file, start_index, end_index, locked_mutexes, &able_to_get_mutexes, READ) != 0, 
      "wait_try_to_get_mutexes failed\n");

    // if we are able to get all mutexes
    if (able_to_get_mutexes) {

      // update number of readers count
      IF_TRUE_THEN_FAILED_TO_READ(update_num_readers(file, start_index, end_index, INCREASE_COUNT) != 0, 
        "update_num_readers failed\n");

      // add zc_access_info entry
      IF_TRUE_THEN_FAILED_TO_READ(add_access_info_entry(file, start_index, end_index) != 0, 
        "add_access_info_entry failed\n");

    // if there exists a mutex that we could not get
    } else {

      // unlock all other mutexes that this thread has locked
      IF_TRUE_THEN_FAILED_TO_READ(unlock_mutexes(file, start_index, end_index, locked_mutexes) != 0, 
        "unlock_mutexes failed\n");
    }

    IF_TRUE_THEN_FAILED_TO_READ(post_try_to_access_buffer_mutex(file) != 0, 
      "post_try_to_access_buffer_mutex failed\n");

    
    free(locked_mutexes);
    locked_mutexes = NULL;
    
    

    if (able_to_get_mutexes) {
      // break out of while loop
      break;
    } else {
      // sleep for 1 second to let other threads go first
      sleep(1);
    }

  }

  // invalid offset
  IF_TRUE_THEN_FAILED_TO_READ((file->offset < 0 || file->offset >= file->size), 
    "invalid offset\n");

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

  IF_TRUE_THEN_EXIT_ONE(wait_try_to_access_buffer_mutex(file) != 0, 
    "wait_try_to_access_buffer_mutex failed\n");

  // get the access_infro_entry
  zc_access_info *ptr = remove_access_info_entry(file);

  IF_TRUE_THEN_EXIT_ONE(ptr == NULL, "get_access_info_entry failed\n");

  int start_index = ptr->start_index;
  int end_index = ptr->end_index;
  if (ptr) {
    free(ptr);
    ptr = NULL;
  }

  for (int i = start_index; i <= end_index; i++) {
    file->num_readers[i]--;
    
    if (file->num_readers[i] == 0) {
      IF_TRUE_THEN_EXIT_ONE(sem_post(&(file->buffer_mutexes[i])) != 0, "sem_post failed\n");
    }
  }
  IF_TRUE_THEN_EXIT_ONE(post_try_to_access_buffer_mutex(file) != 0, 
    "post_try_to_access_buffer_mutex failed\n");
  
}

/**************
 * Exercise 2 *
 **************/

char *zc_write_start(zc_file *file, size_t size) {

  while (1) {

    IF_TRUE_THEN_FAILED_TO_WRITE(wait_try_to_access_buffer_mutex(file) != 0, 
      "wait_try_to_access_buffer_mutex failed\n");
  

    int num_pages = calc_num_pages(file);
    int able_to_get_mutexes = 1;
    int *locked_mutexes = calloc(num_pages, sizeof(int));
    IF_TRUE_THEN_FAILED_TO_WRITE(locked_mutexes == NULL, "malloc failed\n");
    for (int i=0; i < num_pages; i++) {
        locked_mutexes[i] = 0;
    }
    if (locked_mutexes == NULL) {
      return NULL;
    }

    if (file->size == 0) {

      IF_TRUE_THEN_FAILED_TO_WRITE(add_access_info_entry(file, 0, -1) != 0, 
          "add_access_info_entry failed\n");
    } else {

      // take min(file->offset, file->size) as start offset
      long start_offset = (file->offset <= file->size) ? file->offset : file->size;
      int start_index = get_index(start_offset);

      long end_offset = (file->offset + size < file->size) ? file->offset + size -1: file->size -1;
      int end_index = get_index(end_offset);

      // for each of the pages that we need to write
      IF_TRUE_THEN_FAILED_TO_WRITE(try_to_get_mutexes(file, start_index, end_index, locked_mutexes, &able_to_get_mutexes, WRITE) != 0, 
        "wait_try_to_get_mutexes failed\n");

      // if we are not able to get all mutexes
      if (able_to_get_mutexes) {
        IF_TRUE_THEN_FAILED_TO_WRITE(add_access_info_entry(file, start_index, end_index) != 0, 
          "add_access_info_entry failed\n");

      } else {
        // unlock all other mutexes that this thread has locked
        IF_TRUE_THEN_FAILED_TO_WRITE(unlock_mutexes(file, start_index, end_index, locked_mutexes) != 0, 
          "unlock_mutexes failed\n");
      }
    }
    

    IF_TRUE_THEN_FAILED_TO_WRITE(post_try_to_access_buffer_mutex(file) != 0, 
      "post_try_to_access_buffer_mutex failed\n");

    free(locked_mutexes);

    if (able_to_get_mutexes) {
      // break out of while loop
      break;
    } else {
      // sleep for 1 second to let other threads go first
      sleep(1);
    }
  }

  // invalid offset
  if (file->offset < 0) {
    return NULL;
  }

  // check if offset is beyond size of file
  // if it is, fill gap with '\0' characters
  if (file->offset > file->size) {

    IF_TRUE_THEN_FAILED_TO_WRITE(update_file_size(file, file->offset, 1) != 0, 
      "update_file_size failed\n");
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

    IF_TRUE_THEN_FAILED_TO_WRITE(update_file_size(file, new_size, 0) != 0, 
      "update_file_size failed\n");
  }

  // update offset
  file->offset += size;

  // return pointer to original offset
  return file->ptr + old_offset;

}

void zc_write_end(zc_file *file) {

  IF_TRUE_THEN_EXIT_ONE(wait_try_to_access_buffer_mutex(file) != 0, 
      "wait_try_to_access_buffer_mutex failed\n");


  zc_access_info *ptr = remove_access_info_entry(file);
  IF_TRUE_THEN_EXIT_ONE(ptr == NULL, "get_access_info_entry failed\n");

  int start_index = ptr->start_index;
  int end_index = ptr->end_index;
  free(ptr);
  
  // flush updates into file
  if (msync(file->ptr+(start_index * sysconf(_SC_PAGESIZE)), (end_index - start_index + 1) * sysconf(_SC_PAGESIZE), MS_SYNC) != 0) {
    perror("mysnc failed\n");
    exit(1);
  }

  for (int i = start_index; i <= end_index; i++) {

    if (sem_post(&(file->buffer_mutexes[i])) != 0) {

      perror("sem_post failed\n");
      exit(1);
    }
  }

  IF_TRUE_THEN_EXIT_ONE(post_try_to_access_buffer_mutex(file) != 0, 
      "post_try_to_access_buffer_mutex failed\n");

}

/**************
 * Exercise 3 *
 **************/

off_t zc_lseek(zc_file *file, long offset, int whence) {

  IF_TRUE_THEN_FAILED_TO_LSEEK(wait_try_to_access_buffer_mutex(file) != 0, 
    "sem_wait failed\n");

  off_t retval;
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

  IF_TRUE_THEN_FAILED_TO_LSEEK((retval < 0), "lseek retval < 0\n");

  if (retval != (off_t) - 1) {
    file->offset = retval;
  }

  IF_TRUE_THEN_FAILED_TO_LSEEK(post_try_to_access_buffer_mutex(file) != 0, 
    "sem_post failed\n");

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
  const char *read_ptr = zc_read_start(source_zc_file, (size_t *) &read_size);
  if (read_ptr == NULL) {
    return -1;
  }

  if (read_size != source_zc_file->size) {
    return -1;
  }

  if (source_zc_file->size < dest_zc_file->size) {
    long old_size = dest_zc_file->size;
    long new_size = source_zc_file->size;
    // increase size of file
    if (ftruncate(dest_zc_file->fd, new_size) != 0) {
      return -1;
    }
    if (update_ptr_to_virtual_address(dest_zc_file, new_size) != 0) {
      return -1;
    }
    dest_zc_file->size = new_size;

    if (update_sync_resources(dest_zc_file, old_size) != 0) {
      return -1;
    }

  }

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


int update_ptr_to_virtual_address(zc_file *file, long new_size) {
  // if new_size is 0, then don't map into virtual memory
  if (new_size == 0) {
    file->ptr = NULL;  
  }
  else if (file->ptr != NULL) {
    // remap memory
    file->ptr = mremap(file->ptr, file->size, new_size, MREMAP_MAYMOVE);
    if (file->ptr == MAP_FAILED) {
      perror("mremap failed\n");
      return -1;
    }
  }
  else {
    // map memory
    file->ptr = mmap(NULL, new_size, PROT_READ | PROT_WRITE, MAP_SHARED_VALIDATE, file->fd, 0);
    if (file->ptr == MAP_FAILED) {
      perror("mmap failed\n");
      return -1;
    }
  }
  
  return 0;
}

long get_file_size(zc_file *file) {
  struct stat statbuf;
  if (fstat(file->fd, &statbuf) != 0) {
    perror("fstat failed\n");
    return -1;
  }
  return (long) statbuf.st_size;
}

int init_sync_resources(zc_file *file) {
  int num_pages = calc_num_pages(file);

  if (sem_init(&(file->try_to_access_buffer_mutex), 0, 1) != 0) {
    perror("sem_init failed\n");
    return -1;
  }
  

  if (num_pages == 0) {

    file->buffer_mutexes = NULL;
    file->num_readers = NULL;
    return 0;
  }

  file->buffer_mutexes = malloc(sizeof(sem_t) * num_pages);

  if (file->buffer_mutexes == NULL) {
    perror("malloc failed\n");
    return -1;
  }

  for (int i = 0; i < num_pages; i++) {
    if (sem_init(&(file->buffer_mutexes[i]), 0, 1) != 0) {
      perror("sem_init failed\n");
      return -1;
    }
  }
  
  file->num_readers = malloc(sizeof(int) * num_pages);
  if (file->num_readers == NULL) {
    perror("malloc failed\n");
    return -1;
  }
  for (int i = 0; i < num_pages; i++) {
    file->num_readers[i] = 0;
  }

  return 0;

}

int calc_num_pages(zc_file *file) {
  if (file->size % sysconf(_SC_PAGESIZE) == 0) {
    return (int) get_index(file->size);
  } else {
    return (int) get_index(file->size) + 1;
  }
}


int get_index(long num) {
  return (int) (num / sysconf(_SC_PAGESIZE));
}

int add_access_info_entry(zc_file *file, int start_index, int end_index) {

  // create new entry
  zc_access_info *new_ptr = (zc_access_info *) malloc(sizeof(zc_access_info));
  if (new_ptr == NULL) {
    perror("malloc failed\n");
    return -1;
  }
  new_ptr->next = NULL;
  new_ptr->thread_id = gettid();
  new_ptr->start_index = start_index;
  new_ptr->end_index = end_index;

  // get last entry
  zc_access_info *ptr = file->head_ptr;
  if (ptr) {
    while (ptr->next) {
      ptr = ptr->next;
    }

    ptr->next = new_ptr;
  }
  else {
    file->head_ptr = new_ptr;
  }
  return 0;
}

int update_num_readers(zc_file *file, int start_index, int end_index, update_readers_mode mode) {
  switch (mode) {
    case INIT_COUNT:
      for (int i=start_index; i <= end_index; i++) {
        file->num_readers[i] = 0;
      }
      return 0;
    case INCREASE_COUNT:
      for (int i=start_index; i <= end_index; i++) {
        file->num_readers[i]++;
      }
      return 0;
    case DECREASE_COUNT:
      for (int i=start_index; i <= end_index; i++) {
        file->num_readers[i]--;
      }
      return 0;
    default:
      perror("unexpected mode for update_num_readers\n");
      return -1;
  }
}

int unlock_mutexes(zc_file *file, int start_index, int end_index, int *locked_mutexes) {
  for (int i = start_index; i <= end_index; i++) {
    if (locked_mutexes[i]) {
      if (sem_post(&(file->buffer_mutexes[i])) != 0) {
        return -1;
      }
    }
  }
  return 0;
}

int wait_try_to_access_buffer_mutex(zc_file *file) {

  if (sem_wait(&(file->try_to_access_buffer_mutex)) != 0) {
    perror("sem_wait failed\n");
    return -1;
  }

  return 0;
}

int try_to_get_mutexes(zc_file *file, int start_index, int end_index, int *locked_mutexes, int *able_to_get_mutexes, mode mode) {
  for (int i = start_index; i <= end_index; i++) {
    if ((mode == READ && file->num_readers[i] == 0) || (mode == WRITE)) {
      if (sem_trywait(&(file->buffer_mutexes[i])) == 0) {
        // record mutexes that this read has locked
        locked_mutexes[i] = 1; 
      } else {

        if (errno == EAGAIN) {
          // one of the mutex that's needed is already locked
          *able_to_get_mutexes = 0;
          break;
        }
        else {
          return -1;
        }
        
      }
    }
  }
  return 0;
}


int post_try_to_access_buffer_mutex(zc_file *file) {

  if (sem_post(&(file->try_to_access_buffer_mutex)) != 0) {
    perror("sem_post failed\n");
    return -1;
  }
  return 0;

}

void set_start_and_end_index(zc_file *file, size_t* size, int *start_index, int *end_index) {
  *start_index = get_index(file->offset);
  long new_offset = ((file->size - file->offset) >= (long) *size) ? (long) (file->offset + *size) : (long) file->size;
  *end_index = get_index(new_offset-1);
}

zc_access_info *remove_access_info_entry(zc_file *file) {
  pid_t target_thread_id = gettid();
  zc_access_info *ptr = file->head_ptr;

  if (ptr == NULL) {
    perror("file->head_ptr should not be empty\n");
    exit(1);
  }
  zc_access_info *prev = NULL;

  while (ptr) {
    if (ptr->thread_id == target_thread_id) {
      break;
    }
    else {
      prev = ptr;
      ptr = ptr->next;
    }
  }

  if (prev) {
    prev->next = ptr->next;
  } else {
    file->head_ptr = ptr->next;
  }

  return ptr;
}

int update_sync_resources(zc_file *file, long size) {

  int new_num_pages = calc_num_pages(file);
  int num_pages = 0;

  if (size % sysconf(_SC_PAGESIZE) == 0) {
    num_pages = (int) get_index(size);
  } else {
    num_pages = (int) get_index(size) + 1;
  }


  if (new_num_pages > num_pages) {
    // increase length of buffer_mutexes array

    file->buffer_mutexes = realloc(file->buffer_mutexes, new_num_pages * sizeof(sem_t));

    if (file->buffer_mutexes == NULL) {
      return -1;
    }
    // increase length of num_readers array
    file->num_readers = realloc(file->num_readers, new_num_pages * sizeof(int));
    if (file->num_readers == NULL) {
      return -1;
    }

    for (int i = num_pages; i < new_num_pages; i++) {
      // initialize new mutexes
      if (sem_init(&(file->buffer_mutexes[i]), 0, 0) != 0) {
        return -1;
      }

      // initialize new counters
      file->num_readers[i] = 0;
    }


  }
  else if (new_num_pages < num_pages) {


    for (int i = new_num_pages; i < num_pages; i++) {

      if (sem_destroy(&(file->buffer_mutexes[i])) != 0) {
        return -1;
      }
      file->num_readers[i] = 0;
    }

    // decrease length of buffer_mutexes array
    file->buffer_mutexes = realloc(file->buffer_mutexes, new_num_pages * sizeof(sem_t));

    if (file->buffer_mutexes == NULL) {
      return -1;
    }

    // decrease length of num_readers array
    file->num_readers = realloc(file->num_readers, new_num_pages* sizeof(int));
    if (file->num_readers == NULL) {
      return -1;
    }


  }

  return 0;
}


int update_access_info_entry(zc_file *file, int end_index) {
  pid_t target_thread_id = gettid();
  zc_access_info *ptr = file->head_ptr;

  if (ptr == NULL) {
    perror("file->head_ptr should not be empty\n");
    exit(1);
  }

  while (ptr) {
    if (ptr->thread_id == target_thread_id) {
      break;
    }
    else {
      ptr = ptr->next;
    }
  }

  ptr->end_index = end_index;

  return 0;
}

int update_file_size(zc_file *file, long new_size, int fill_with_null) {
  long old_size = file->size;


  // increase size of file
  if (ftruncate(file->fd, new_size) != 0) {
    return -1;
  }


  if (update_ptr_to_virtual_address(file, new_size) != 0) {
    return -1;
  }
  file->size = new_size;

  if (fill_with_null) {
    memset(file->ptr+old_size, 0, file->offset-old_size);
  }

  if (update_sync_resources(file, old_size) != 0) {
    return -1;
  }

  int end_index = get_index(new_size);
  if (update_access_info_entry(file, end_index) != 0) {
    return -1;
  } 

  return 0;
}

