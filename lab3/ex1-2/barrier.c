/**
 * CS2106 AY 20/21 Semester 1 - Lab 3
 *
 * This file contains function definitions. Your implementation should go in
 * this file.
 */
#include <semaphore.h>
#include <stdlib.h>

#include "barrier.h"

// Initialise barrier here
void barrier_init ( barrier_t *barrier, int count ) {
    barrier->count = count;
    barrier->waitQ = malloc(sizeof(sem_t));
    sem_init( barrier->waitQ, 0, 0);

    barrier->mutex = malloc(sizeof(sem_t));
    sem_init( barrier->mutex, 0, 1);
}

void barrier_wait ( barrier_t *barrier ) {

	sem_wait(barrier->mutex);
	barrier->count --;
	sem_post(barrier->mutex);


	if (barrier->count == 0) {
		sem_post(barrier->waitQ);
	}
	sem_wait(barrier->waitQ);
	sem_post(barrier->waitQ);

}

// Perform cleanup here if you need to
void barrier_destroy ( barrier_t *barrier ) {
	free(barrier->waitQ);
}
