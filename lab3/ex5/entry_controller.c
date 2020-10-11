/**
 * CS2106 AY 20/21 Semester 1 - Lab 3
 *
 * Your implementation should go in this file.
 */

#include "entry_controller.h"

void entry_controller_init( entry_controller_t *entry_controller, int loading_bays ) {
	// records of all the trains
	// each trains will have their own semaphore in records, regardless of whether
	// they are queued or directly enter the loading bay 
	entry_controller->records = malloc(sizeof(sem_t *) * (ENTRY_CONTROLLER_MAX_USES));
	for (int i=0; i < ENTRY_CONTROLLER_MAX_USES; i++) {
		entry_controller->records[i] = malloc(sizeof(sem_t));
		if (entry_controller->records[i] == NULL) {
			perror("Failed to allocate memory\n");
			exit(1);
		}
		if (sem_init(entry_controller->records[i], 0, 0) != 0) {
			perror("Failed to initialise semaphore\n");
			exit(1);
		}
	}
	// mutex to synchronize the update of shared variables
	entry_controller->mutex = malloc(sizeof(sem_t));
	if (entry_controller->mutex == NULL) {
		perror("Failed to allocate memory");
		exit(1);
	}
	if (sem_init(entry_controller->mutex, 0, 1) != 0) {
		perror("Failed to initialise semaphore\n");
		exit(1);
	}

	// number of trains in queue
	entry_controller->count = 0;

	// index of the first train to exit the queue
	entry_controller->first = 0;

	// index for the next train to be added into records
	entry_controller->last = 0;

	// number of trains in the loading bay
	entry_controller->occupied_loading_bays = 0;

	// max capacity of loading bay
	entry_controller->loading_bays = loading_bays;
}

// train sends request to enter loading bay
void entry_controller_wait( entry_controller_t *entry_controller ) {
	int enter_queue = 0;
	int index = 0;

	if (sem_wait(entry_controller->mutex) != 0) {
		perror("Failed to wait semaphore\n");
		exit(1);
	}

	if (entry_controller->count == ENTRY_CONTROLLER_MAX_USES) {
		perror("Entry queue is full");
		exit(1);
	}

	// get index for train
	index = entry_controller->last;

	// update last for next train
	entry_controller->last = ((entry_controller->last + 1) % ENTRY_CONTROLLER_MAX_USES);

	// if loading bay is full or queue is not empty, add into queue
	enter_queue = (int) ((entry_controller->count != 0) || (entry_controller->occupied_loading_bays == entry_controller->loading_bays));
	
	// assume that train will stay in queue, increase count by 1
	entry_controller->count += 1;

	if (!enter_queue) {
		entry_controller->first = ((entry_controller->first + 1) % ENTRY_CONTROLLER_MAX_USES);
		entry_controller->count--;
		entry_controller->occupied_loading_bays++;
	}

	if (sem_post(entry_controller->mutex) != 0) {
		perror("Failed to post semaphore\n");
		exit(1);
	}

	// if loading bay is full, train will wait on its semaphore
	if (enter_queue) {

		if (sem_wait(entry_controller->records[index]) != 0) {
			perror("Failed to wait semaphore\n");
			exit(1);
		}
	}	
}

// train sends request to exit loading bay
void entry_controller_post( entry_controller_t *entry_controller ) {
	if(sem_wait(entry_controller->mutex) != 0) {
		perror("Failed to wait semaphore\n");
		exit(1);
	}

	// if queue is not empty, add train from queue into loading bay 
	if (entry_controller->count != 0) {
		// allow first train in queue to proceed 
		if (sem_post(entry_controller->records[entry_controller->first]) != 0) {
			perror("Failed to post semaphore\n");
			exit(1);
		}

		// update first
		entry_controller-> first = ((entry_controller->first + 1) % ENTRY_CONTROLLER_MAX_USES);

		// update count
		entry_controller->count--;

		// number of trains in loading bay remains the same
	}
	// if no train from queue is added, 
	// decrease the number of trains in the loading bay
	else {
		entry_controller->occupied_loading_bays--;
	}

	if (sem_post(entry_controller->mutex) != 0) {
		perror("Failed to post semaphore\n");
		exit(1);
	}
}

void entry_controller_destroy( entry_controller_t *entry_controller ) {
	// destroy all mutexes and the memory allocated in records
	for (int i=0; i < ENTRY_CONTROLLER_MAX_USES; i++) {
		if (sem_destroy(entry_controller->records[i]) != 0) {
			perror("Failed to destroy semaphore");
			exit(1);
		}
		free(entry_controller->records[i]);
	}

	// free memory allocated to records
	free(entry_controller->records);

	// destroy mutex
	if (sem_destroy(entry_controller->mutex) != 0) {
		perror("Failed to destroy semaphore");
		exit(1);
	}

	// free memory allocated for mutex
	free(entry_controller->mutex);
}

