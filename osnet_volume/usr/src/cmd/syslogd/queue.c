/*
 * Copyright(c) 1996-1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)queue.c 1.2	99/04/19 SMI"

#include <pthread.h>
#include <malloc.h>
#include <memory.h>
#include "dataq.h"
#include <assert.h>

static int
dataq_check(dataq_t *ptr)	/* call while holding lock! */
{
	assert(ptr->num_data == ll_check(&ptr->data));
	assert(ptr->num_waiters == ll_check(&ptr->waiters));
	return (1);
}
int
dataq_init(dataq_t *ptr)
{
	ptr->num_data = 0;
	ptr->num_waiters = 0;
	ll_init(&ptr->data);
	ll_init(&ptr->waiters);
	pthread_mutex_init(&ptr->lock, NULL);
	assert((pthread_mutex_lock(&ptr->lock) == 0) &&
		(dataq_check(ptr) == 1) &&
		(pthread_mutex_unlock(&ptr->lock) == 0));
	return (0);
}
int
dataq_enqueue(dataq_t *dataq, void *in)
{
	dataq_data_t *ptr = (dataq_data_t *) malloc(sizeof (*ptr));
	dataq_waiter_t *sleeper = NULL;

	if (ptr == NULL)
		return (-1);
	ptr->data = in;
	pthread_mutex_lock(&dataq->lock);
	assert(dataq_check(dataq));
	ll_enqueue(&dataq->data, &ptr->list);
	dataq->num_data++;
	if (dataq->num_waiters) {
		sleeper = (dataq_waiter_t *) ll_peek(&dataq->waiters);
		sleeper->wakeup = 1;
		pthread_cond_signal(&sleeper->cv);
	}
	assert(dataq_check(dataq));
	pthread_mutex_unlock(&dataq->lock);
	return (0);
}
int
dataq_dequeue(dataq_t *dataq, void **outptr)
{
	dataq_data_t *dptr;
	dataq_waiter_t *sleeper = NULL;

	pthread_mutex_lock(&dataq->lock);
	if ((dataq->num_waiters > 0) ||
	    ((dptr = (dataq_data_t *) ll_dequeue(&dataq->data)) == NULL)) {
		dataq_waiter_t wait;
		wait.wakeup = 0;
		pthread_cond_init(&wait.cv, NULL);
		dataq->num_waiters++;
		ll_enqueue(&dataq->waiters, &wait.list);
		while (wait.wakeup == 0)
			pthread_cond_wait(&wait.cv, &dataq->lock);
		ll_dequeue(&dataq->waiters);
		dataq->num_waiters--;
		pthread_cond_destroy(&wait.cv);
		dptr = (dataq_data_t *) ll_dequeue(&dataq->data);
	}
	dataq->num_data--;
	if (dataq->num_data && dataq->num_waiters) {
		sleeper = (dataq_waiter_t *) ll_peek(&dataq->waiters);
		sleeper->wakeup = 1;
		pthread_cond_signal(&sleeper->cv);
	}
	pthread_mutex_unlock(&dataq->lock);
	*outptr = dptr->data;
	free(dptr);
	return (0);
}
static void
dataq_data_destroy(void * p)
{
	dataq_data_t * d = (dataq_data_t *) p;
	free(d->data);
	free(d);
}
static void
dataq_waiters_destroy(void * p)
{
	dataq_waiter_t * d = (dataq_waiter_t *) p;
	pthread_cond_destroy(&d->cv);
	free(d);
}
int
dataq_destroy(dataq_t *dataq)
{
	pthread_mutex_destroy(&dataq->lock);
	ll_mapf(&dataq->data, dataq_data_destroy);
	ll_mapf(&dataq->waiters, dataq_waiters_destroy);
	return (0);
}
