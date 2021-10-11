/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)list.c 1.2	99/04/19 SMI"

#include <pthread.h>
#include <malloc.h>
#include <memory.h>
#include <assert.h>
#include <poll.h>
#include <stdio.h>
#include "llt.h"
void
ll_init(llh_t *head)
{
	head->back = &head->front;
	head->front = NULL;
}
void
ll_enqueue(llh_t *head, ll_t *data)
{
	data->n = NULL;
	*head->back = data;
	head->back = &data->n;
}
/*
 * apply the function func to every element of the ll in sequence.  Can
 * be used to free up the element, so "n" is computed before func is
 * called on it.
 */
void
ll_mapf(llh_t *head, void (*func)(void *))
{
	ll_t * t = head->front;
	ll_t * n;

	while (t) {
		n = t->n;
		func(t);
		t = n;
	}
}
ll_t *
ll_peek(llh_t *head)
{
	return (head->front);
}
ll_t *
ll_dequeue(llh_t *head)
{
	ll_t *ptr;
	ptr = head->front;
	if (ptr && ((head->front = ptr->n) == NULL))
		head->back = &head->front;
	return (ptr);
}
ll_t *
ll_traverse(llh_t *ptr, int (*func)(void *, void *), void *user)
{
	ll_t *t;
	ll_t **prev = &ptr->front;

	t = ptr->front;
	while (t) {
		switch (func(t, user)) {
		case 1:
			return (NULL);
		case 0:
			prev = &(t->n);
			t = t->n;
			break;
		case -1:
			if ((*prev = t->n) == NULL)
				ptr->back = prev;
			return (t);
		}
	}
	return (NULL);
}
/* Make sure the list isn't corrupt and returns number of list items */
int
ll_check(llh_t *head)
{
	int i = 0;
	ll_t *ptr = head->front;
	ll_t **prev = &head->front;

	while (ptr) {
		i++;
		prev = &ptr->n;
		ptr = ptr->n;
	}
	assert(head->back == prev);
	return (i);
}
