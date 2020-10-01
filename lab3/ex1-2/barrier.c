/**
 * CS2106 AY 20/21 Semester 1 - Lab 3
 *
 * This file contains function definitions. Your implementation should go in
 * this file.
 */

#include "barrier.h"

// Initialise barrier here
void barrier_init ( barrier_t *barrier, int count ) {
    barrier->count = count;
}

void barrier_wait ( barrier_t *barrier ) {

}

// Perform cleanup here if you need to
void barrier_destroy ( barrier_t *barrier ) {
}
