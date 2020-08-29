/*************************************
* Lab 1 Exercise 2
* Name:
* Student No:
* Lab Group:
*************************************/

#include "functions.h"

// write the necessary code to initialize the func_list
// array here, if needed
#include "function_pointers.h"

void update_functions() 
{
    *(func_list) = add_one;
    *(func_list + 1) = add_two;
    *(func_list + 2) = multiply_five;
    *(func_list + 3) = square;
    *(func_list + 4) = cube;
}
