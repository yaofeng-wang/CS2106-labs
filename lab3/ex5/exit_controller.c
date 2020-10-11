/**
 * CS2106 AY 20/21 Semester 1 - Lab 3
 *
 * This file contains declarations. You should only modify the fifo_sem_t struct,
 * as the method signatures will be needed to compile with the runner.
 */

#define EXIT_QUEUE_SIZE 51 // 1 + max num trains in loading bay because we need the buffer
                           // to indicate when the queue is full 
#include "exit_controller.h"

void exit_controller_init(exit_controller_t *exit_controller, int no_of_priorities) {
	exit_controller->no_of_priorities = no_of_priorities;

	// circular queue for each priority
	exit_controller->MLQ = malloc(sizeof(sem_t**) * no_of_priorities);
	if (exit_controller->MLQ == NULL) {
		perror("Failed to allocate memory\n");
		exit(1);
	}

	for (int i = 0; i < no_of_priorities; i++) {
		exit_controller->MLQ[i] = malloc(sizeof(sem_t*) * EXIT_QUEUE_SIZE);
		if (exit_controller->MLQ[i] == NULL) {
			perror("Failed to allocate memory\n");
			exit(1);
		}
		
		for (int j = 0; j < EXIT_QUEUE_SIZE; j++) {
			exit_controller->MLQ[i][j] = malloc(sizeof(sem_t));
			if (exit_controller->MLQ[i][j] == NULL) {
				perror("Failed to allocate memory\n");
				exit(1);
			}
			if (sem_init(exit_controller->MLQ[i][j], 0, 0) != 0) {
				perror("Failed to initialise semaphore\n");
				exit(1);
			}
		}
	}
	

	// index of first train to exit
	exit_controller->first = calloc(no_of_priorities, sizeof(int));
	if (exit_controller->first == NULL) {
		perror("Failed to allocate memory\n");
		exit(1);
	}

	// index of next train to be added
	exit_controller->last = calloc(no_of_priorities, sizeof(int));
		if (exit_controller->last == NULL) {
		perror("Failed to allocate memory\n");
		exit(1);
	}

	// mutex to synchronize modification of shared variables
	exit_controller->mutex = malloc(sizeof(sem_t));
	if (exit_controller->mutex == NULL) {
		perror("Failed to allocate memory");
		exit(1);
	}
	if (sem_init(exit_controller->mutex, 0, 1) != 0) {
		perror("Failed to initialise semaphore\n");
		exit(1);
	}
				
	// flag indicates whether exit line is empty
	exit_controller->exit_line_empty = 1;
}

// train sents request to enter exit line
void exit_controller_wait(exit_controller_t *exit_controller, int priority) {

	int enter_queue = 0;
	int index = 0;
	sem_wait(exit_controller->mutex);

	// if exit line is empty, then exit_line_empty should be false
	if (exit_controller->exit_line_empty) {

		// also indicates whether the queue is empty for all priorities
		// in practice, only the first train will hit this line
		exit_controller->exit_line_empty = 0;

	}
	// if exit line is not empty and the queue is not full
	// set enter_queue to true
	// and get index for train
	// and update last variable 
	else if (((exit_controller->last[priority] + 1) % EXIT_QUEUE_SIZE) !=  exit_controller->first[priority]){
		enter_queue = 1;
		index = exit_controller->last[priority];
		exit_controller->last[priority] = (exit_controller->last[priority] + 1) % EXIT_QUEUE_SIZE; 
	}

	else {
		perror("Exit queue is full\n");
		exit(1);

	}
	sem_post(exit_controller->mutex);

	// if enter_queue is true, then make the train wait at their index
	if (enter_queue) {
		sem_wait(exit_controller->MLQ[priority][index]);
	}
	// if enter_queue is false, then do nothing
}

// train sents request to exit exit line
void exit_controller_post(exit_controller_t *exit_controller, int priority) {
	(void) priority;

	int foundTrain = 0;
	sem_wait(exit_controller->mutex);

	// always let trains with higher priority (lower priority value)
	// go first
	for (int i = 0; i < exit_controller->no_of_priorities; i++) {
		if (exit_controller->first[i] != exit_controller->last[i]) {

			foundTrain = 1;
			sem_post(exit_controller->MLQ[i][exit_controller->first[i]]);
			exit_controller->first[i] = (exit_controller->first[i] + 1) % EXIT_QUEUE_SIZE;
			break;
		}

	}

	// if all queues are empty, then let exit_line_empty be true
	if (!foundTrain) {
		exit_controller->exit_line_empty = 1;
	}
	sem_post(exit_controller->mutex);
}

void exit_controller_destroy(exit_controller_t *exit_controller){
	
	// destroy mutex in MLQ
	for (int i = 0; i < exit_controller->no_of_priorities; i++) {
		for (int j = 0; j < EXIT_QUEUE_SIZE; j++) {
			if (sem_destroy(exit_controller->MLQ[i][j]) != 0) {
				perror("Failed to destroy semaphore");
				exit(1);
			}
			free(exit_controller->MLQ[i][j]);
			exit_controller->MLQ[i][j] = NULL;
		}
		free(exit_controller->MLQ[i]);
		exit_controller->MLQ[i] = NULL;
	}

	free(exit_controller->first);

	free(exit_controller->last);

	free(exit_controller->MLQ);

	// destroy mutex 
	if (sem_destroy(exit_controller->mutex) != 0) {
		perror("Failed to destroy semaphore");
		exit(1);
	}

	// free memory allocated for mutex
	free(exit_controller->mutex);
}
// ./train_runner 4999 50 2 1 : 
// ./train_runner 4999 50 2 2 : 
// ./train_runner 4999 50 2 3 : 
