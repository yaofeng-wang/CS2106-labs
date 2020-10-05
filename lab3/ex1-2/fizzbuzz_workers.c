/**
 * CS2106 AY 20/21 Semester 1 - Lab 3
 *
 * Your implementation should go in this file.
 */

#include<semaphore.h>
#include<stdlib.h>
#include <stdio.h>
#include "fizzbuzz_workers.h"
#include "barrier.h" // you may use barriers if you think it can help your
                     // implementation

// declare variables to be used here
int NUM_THREADS = 4;
int cur_n;
int max_n;
sem_t *mutex;
sem_t **task;

void fizzbuzz_init ( int n ) {
	cur_n = 1;
	max_n = n;
	mutex = malloc(sizeof(sem_t));
    sem_init( mutex, 0, 1 );

    task = malloc(sizeof(sem_t*) * 4);
    for (int i = 0; i < 4; i++) {
    	task[i] = malloc(sizeof(sem_t));
    	if (i == 0) {
    		sem_init(task[i], 0, 1);
    	}
    	else {
    		sem_init(task[i], 0, 0);
    	}
    }
    // 0 for num_thread
    // 1 for fizz_thread
    // 2 for buzz_thread
    // 3 for fizzbuzz_thread

}

void num_thread( int n, void (*print_num)(int) ) {
	(void) n;

	while (cur_n <= max_n) {

		sem_wait(task[0]);

		// printf("From num_thread, cur_n: %d\n", cur_n);

		if ((cur_n % 3 != 0) && (cur_n % 5 != 0) && (cur_n <= max_n)) {
			print_num(cur_n);
			
		}
		cur_n ++;

		
		if (cur_n > max_n) {
			for (int i = 0; i < 4; i++) {
				sem_post(task[i]);
			}
			break;
		}
		else if (cur_n % 5 == 0 && cur_n % 3 == 0) {
			sem_post(task[3]);
		}
		else if (cur_n % 3 == 0) {
			sem_post(task[1]);
		}
		else if (cur_n % 5 == 0) {
			sem_post(task[2]);
		}
		else {
			sem_post(task[0]);
		}
	}
}

void fizz_thread( int n, void (*print_fizz)(void) ) {
	(void) n;

	while (cur_n <= max_n) {

		sem_wait(task[1]);

		// sem_wait(mutex);


		if ((cur_n % 3 == 0) && (cur_n % 5 != 0) && (cur_n <= max_n)) {
			print_fizz();
		}
		cur_n ++;


		if (cur_n > max_n) {
			for (int i = 0; i < 4; i++) {
				sem_post(task[i]);
			}
			break;
		}

		else if (cur_n % 5 == 0 && cur_n % 3 == 0) {
			sem_post(task[3]);
		}
		else if (cur_n % 3 == 0) {
			sem_post(task[1]);
		}
		else if (cur_n % 5 == 0) {
			sem_post(task[2]);
		}
		else {
			sem_post(task[0]);
		}

		// sem_post(mutex);

	}
}

void buzz_thread( int n, void (*print_buzz)(void) ) {
	(void) n;

	while (cur_n <= max_n) {
		
		sem_wait(task[2]);

		// sem_wait(mutex);


		if ((cur_n % 3 != 0) && (cur_n % 5 == 0) && (cur_n <= max_n)) {
			print_buzz();
			
		}
		cur_n ++;

		if (cur_n > max_n) {
			for (int i = 0; i < 4; i++) {
				sem_post(task[i]);
			}
			break;
		}
		else if (cur_n % 5 == 0 && cur_n % 3 == 0) {
			sem_post(task[3]);
		}
		else if (cur_n % 3 == 0) {
			sem_post(task[1]);
		}
		else if (cur_n % 5 == 0) {
			sem_post(task[2]);
		}
		else {
			sem_post(task[0]);
		}

		// sem_post(mutex);
	}
}

void fizzbuzz_thread( int n, void (*print_fizzbuzz)(void) ) {
	(void) n;

	while (cur_n <= max_n) {
		
		sem_wait(task[3]);

		// sem_wait(mutex);

		if ((cur_n % 3 == 0) && (cur_n % 5 == 0) && (cur_n <= max_n)) {
			print_fizzbuzz();
			
		}
		cur_n ++;

		if (cur_n > max_n) {
			for (int i = 0; i < 4; i++) {
				sem_post(task[i]);
			}
			break;
		}

		else if (cur_n % 5 == 0 && cur_n % 3 == 0) {
			sem_post(task[3]);
		}
		else if (cur_n % 3 == 0) {
			sem_post(task[1]);
		}
		else if (cur_n % 5 == 0) {
			sem_post(task[2]);
		}
		else {
			sem_post(task[0]);
		}

		// sem_post(mutex);
	}
}

void fizzbuzz_destroy() {

	for (int i = 0; i < 4; i ++) {
		sem_destroy(task[i]);
		free(task[i]);
	}

	free(task);

	sem_destroy(mutex);

	free(mutex);
}
