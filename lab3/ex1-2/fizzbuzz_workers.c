/**
 * CS2106 AY 20/21 Semester 1 - Lab 3
 *
 * Your implementation should go in this file.
 */

#include<semaphore.h>
#include<stdlib.h>

#include "fizzbuzz_workers.h"
#include "barrier.h" // you may use barriers if you think it can help your
                     // implementation

// declare variables to be used here
int NUM_THREADS = 4;
int cur_n;
int max_n;
sem_t *mutex;

void fizzbuzz_init ( int n ) {
	cur_n = 1;
	max_n = n;
	mutex = malloc(sizeof(sem_t));
    sem_init( mutex, 0, 1 );
}

void num_thread( int n, void (*print_num)(int) ) {
	(void) n;

	while (cur_n <= max_n) {

		sem_wait(mutex);

		if ((cur_n % 3 != 0) && (cur_n % 5 != 0) && (cur_n <= max_n)) {
			print_num(cur_n);
			cur_n ++;
		}

		sem_post(mutex);

	}
}

void fizz_thread( int n, void (*print_fizz)(void) ) {
	(void) n;

	while (cur_n <= max_n) {

		sem_wait(mutex);

		if ((cur_n % 3 == 0) && (cur_n % 5 != 0) && (cur_n <= max_n)) {
			print_fizz();
			cur_n ++;
		}

		sem_post(mutex);
	}

}

void buzz_thread( int n, void (*print_buzz)(void) ) {
	(void) n;

	while (cur_n <= max_n) {
		
		sem_wait(mutex);

		if ((cur_n % 3 != 0) && (cur_n % 5 == 0) && (cur_n <= max_n)) {
			print_buzz();
			cur_n ++;
		}

		sem_post(mutex);
	}
}

void fizzbuzz_thread( int n, void (*print_fizzbuzz)(void) ) {
	(void) n;

	while (cur_n <= max_n) {
		
		sem_wait(mutex);

		if ((cur_n % 3 == 0) && (cur_n % 5 == 0) && (cur_n <= max_n)) {
			print_fizzbuzz();
			cur_n ++;
		}



		sem_post(mutex);
	}
}

void fizzbuzz_destroy() {
	free(mutex);

}
