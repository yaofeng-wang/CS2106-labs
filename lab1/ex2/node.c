/*************************************
* Lab 1 Exercise 2
* Name:
* Student No:
* Lab Group:
*************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "node.h"

// copy in your implementation of the functions from ex1
// there is on extra function called map which you have to fill up too
// feel free to add any new functions as you deem fit

// inserts a new node with data value at index (counting from the front
// starting at 0)
void insert_node_from_head_at(list *lst, int index, int data)
{
	// create new node
	node *nn = (node*) malloc(sizeof(node));
	nn->data = data;

	// if the head of list is NULL, then the list is empty
	// assume that index is 0
	if (lst->head == NULL) {
		nn-> prev = NULL;
		nn-> next = NULL;

		lst->head = nn;
		lst->tail = nn;
	} 
	// list is non-empty
	else {

		if (index == 0) {
			node *r = lst->head;
			
			nn->prev = NULL;
			nn->next = r;

			lst->head = nn;
			r->prev = nn;
		}

		else {
			// move p to the node just before the insert index of new node
			node *l = lst->head;
			node *r;
			for (int i = 0; i < index-1; i++) {
				l = l->next; 
			}
			nn->prev = l;
			r = l->next;
			nn->next = r;

			// if p is pointing to the current last node
			if (r == NULL) {
				lst->tail = nn;
			}
			else {
				r->prev = nn;
			}
			l->next = nn;
		}
	}
}

// inserts a new node with data value at index (counting from the back
// starting at 0)
void insert_node_from_tail_at(list *lst, int index, int data)
{
	//create new node
	node *nn = (node*) malloc(sizeof(node));
	nn->data = data;

	// if the head of list is NULL, then the list is empty
	// assume that index is 0
	if (lst->head == NULL) {
		nn-> prev = NULL;
		nn-> next = NULL;

		lst->head = nn;
		lst->tail = nn;
	} 
	// list is non-empty
	else {
		if (index == 0) {
			node *l = lst->tail;
			
			nn->prev = l;
			nn->next = NULL;

			lst->tail = nn;
			l->next = nn;
		}
		else {
			// move r to the node just before the insert index of new node
			node *r = lst->tail;
			node *l;
			for (int i = 0; i < index-1; i++) {
				r = r->prev; 
			}
			nn->next = r;
			l = r->prev;
			nn->prev = l;
			// if p is pointing to the current last node
			if (l == NULL) {
				lst->head = nn;
			}
			else {
				l->next = nn;
			}
			r->prev = nn;
		}
	}
}

// deletes node at index counting from the front (starting from 0)
// note: index is guaranteed to be valid
void delete_node_from_head_at(list *lst, int index)
{
	node *n;
	if (index == 0) {
		n = lst->head;
		node *r = n->next;

		if (r == NULL) {
			lst->head = NULL;
			lst->tail = NULL;
		} else {
			lst->head = r;
			r->prev = NULL;
		}
		
	}
	else {
		// move l to the node just to the left of insertion point
		n = lst->head;
		node *r;
		node *l;
		for (int i = 0; i < index; i++) {
			n = n->next; 
		}
		l = n->prev;
		r = n->next;
		// if l is pointing to the current last node
		if (r == NULL) {
			lst->tail = l;
		}
		else {
			r->prev = l;
		}
		l->next = r;
	}
	free(n);
	n = NULL;
}

// deletes node at index counting from the back (starting from 0)
// note: index is guaranteed to be valid
void delete_node_from_tail_at(list *lst, int index)
{
// 	printf("start: delete_node_from_tail_at\n");
	node *n;
	if (index == 0) {

		n = lst->tail;
		node *l = n->prev;

		// if there is only 1 node in lst
		if (l == NULL) {
			lst->head = NULL;
			lst->tail = NULL;
		}
		else {
			lst->tail = l;
			l->next = NULL;
		}
	}
	else {
		n = lst->tail;
		node *r;
		node *l;
		for (int i = 0; i < index; i++) {
			n = n->prev; 
		}
		l = n->prev;
		r = n->next;
		if (l == NULL) {
			lst->head = r;
		}
		else {
			l->next = r;
		}
		r->prev = l;
	}
	free(n);
	n = NULL;
	// printf("end: delete_node_from_tail_at\n");
}

// resets list to an empty state (no nodes) and frees any allocated memory in
// the process
void reset_list(list *lst)
{
	// list is already empty
	if (lst->head == NULL) {
		return;
	}
	node *n = lst->head;
	node *r;
	while (n != NULL) {
		r = n->next;
		free(n);
		n = r;
	}
	lst->head = NULL;
	lst->tail = NULL;
}

// traverses list and applies func on data values of all elements in the list
void map(list *lst, int (*func)(int))
{
	node* cur_node = lst->head;

	while (cur_node != NULL) {
		cur_node->data = func(cur_node->data);
		cur_node = cur_node->next;
	}

}

// traverses list and returns the sum of the data values of every node
// in the list
long sum_list(list *lst)
{
	long sum = 0;
	node* cur_node = lst->head;

	while (cur_node != NULL) {
		sum += cur_node->data;
		cur_node = cur_node->next;
	}
	return sum;
}
