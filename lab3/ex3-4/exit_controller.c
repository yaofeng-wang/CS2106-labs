/**
 * CS2106 AY 20/21 Semester 1 - Lab 3
 *
 * This file contains declarations. You should only modify the fifo_sem_t struct,
 * as the method signatures will be needed to compile with the runner.
 */

#define EXIT_QUEUE_SIZE 51
#include <stdio.h>

#include "exit_controller.h"

void exit_controller_init(exit_controller_t *exit_controller, int no_of_priorities) {
	// printf("Start exit_controller_init\n");
	(void) no_of_priorities;


	
	// circular queue for priority 0 
	exit_controller->priority_0 = malloc(sizeof(sem_t*) * EXIT_QUEUE_SIZE);
	for (int i=0; i < EXIT_QUEUE_SIZE; i++) {
		exit_controller->priority_0[i] = malloc(sizeof(sem_t));
		if (exit_controller->priority_0[i] == NULL) {
			perror("Failed to allocate memory\n");
			exit(1);
		}
		if (sem_init(exit_controller->priority_0[i], 0, 0) != 0) {
			perror("Failed to initialise semaphore\n");
			exit(1);
		}

	}


	// index of first train to exit
	exit_controller->first_0 = 0;
	// index of next train to be added
	exit_controller->last_0 = 0;

	// circular queue for priority 1
	exit_controller->priority_1 = malloc(sizeof(sem_t*) * EXIT_QUEUE_SIZE);
	for (int i=0; i < EXIT_QUEUE_SIZE; i++) {
		exit_controller->priority_1[i] = malloc(sizeof(sem_t));
		if (exit_controller->priority_1[i] == NULL) {
			perror("Failed to allocate memory\n");
			exit(1);
		}
		if (sem_init(exit_controller->priority_1[i], 0, 0) != 0) {
			perror("Failed to initialise semaphore\n");
			exit(1);
		}
	}

	// index of first train to exit
	exit_controller->first_1 = 0;
	// index of next train to be added
	exit_controller->last_1 = 0;


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
	

	// printf("End exit_controller_init\n");


}

// train sents request to enter exit line
void exit_controller_wait(exit_controller_t *exit_controller, int priority) {
	// printf("Start exit_controller_wait\n");
	int enter_queue = 0;
	int index = 0;
	sem_wait(exit_controller->mutex);

	// if exit line is empty, then exit_line_empty should be false
	if (exit_controller->exit_line_empty) {
		exit_controller->exit_line_empty = 0;

	}

	// if exit line is not empty, then get the enter_queue is true
	// and get index for train
	// and update last variable 
	else if (priority == 0 && (((exit_controller->last_0 + 1) % EXIT_QUEUE_SIZE) !=  exit_controller->first_0)){
		enter_queue = 1;
		index = exit_controller->last_0;
		exit_controller->last_0 = (exit_controller->last_0 + 1) % EXIT_QUEUE_SIZE; 
	}
	else if (priority == 1 && (((exit_controller->last_1 + 1) % EXIT_QUEUE_SIZE) !=  exit_controller->first_1)){
		enter_queue = 1;
		index = exit_controller->last_1;
		exit_controller->last_1 = (exit_controller->last_1 + 1) % EXIT_QUEUE_SIZE; 
	}
	else {
		perror("Exit queue is full\n");
		exit(1);

	}
	sem_post(exit_controller->mutex);

	// if enter_queue is true, then make the train wait at their index
	if (enter_queue) {
		if (priority == 0) {
			sem_wait(exit_controller->priority_0[index]);
		} 
		else if (priority == 1){
			sem_wait(exit_controller->priority_1[index]);
		}
		else {
			perror("Unexpected priority\n");
			exit(1);
		}

	}
	// if enter_queue is false, then do nothing


	// printf("End exit_controller_wait\n");
}

// train sents request to exit exit line
void exit_controller_post(exit_controller_t *exit_controller, int priority) {
	// printf("Start exit_controller_post\n");

	(void) priority;

	sem_wait(exit_controller->mutex);

	// always let trains with higher priority (lower priority value)
	// go first

	// if priority_0 is not empty, 
	// then allow first train in priority_0 to proceed
	if (exit_controller->first_0 != exit_controller->last_0) {
		sem_post(exit_controller->priority_0[exit_controller->first_0]);
		exit_controller->first_0 = (exit_controller->first_0 + 1) % EXIT_QUEUE_SIZE;
	}
	// if priority_1 is not empty, 
	// then allow first train in priority_1 to proceed
	else if (exit_controller->first_1 != exit_controller->last_1) {
		sem_post(exit_controller->priority_1[exit_controller->first_1]);
		exit_controller->first_1 = (exit_controller->first_1 + 1) % EXIT_QUEUE_SIZE;
	}
	// if both queues are empty, then let exit_line_empty to be true
	else {
		exit_controller->exit_line_empty = 1;
	}

	sem_post(exit_controller->mutex);

	// printf("End exit_controller_post\n");
}

void exit_controller_destroy(exit_controller_t *exit_controller){
	
	// printf("Start exit_controller_destroy\n");

	// destroy mutex in priority_0
	for (int i=0; i < EXIT_QUEUE_SIZE; i++) {

		if (sem_destroy(exit_controller->priority_0[i]) != 0) {
			perror("Failed to destroy semaphore");
			exit(1);
		}

		// currently unable to free memory
		// free(exit_controller->priority_0[i]);

	}
	
	// free memory allocated for priority_0
	//free(exit_controller->priority_0);

	// destroy mutex in priority_1
	for (int i=0; i < EXIT_QUEUE_SIZE; i++) {
		if (sem_destroy(exit_controller->priority_1[i]) != 0) {
			perror("Failed to destroy semaphore");
			exit(1);
		}
		free(exit_controller->priority_1[i]);
	}

	// free memory allocated for priority_1
	free(exit_controller->priority_1);

	// destroy mutex 
	if (sem_destroy(exit_controller->mutex) != 0) {
		perror("Failed to destroy semaphore");
		exit(1);
	}

	// free memory allocated for mutex
	free(exit_controller->mutex);

	// printf("End exit_controller_destroy\n");

}
// ./train_runner 4999 50 2 1 : OK
// ./train_runner 4999 50 2 2 : OK
// ./train_runner 4999 50 2 3 : OK
