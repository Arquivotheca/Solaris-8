/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)timer_queue.c	1.3	99/09/21 SMI"

#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stropts.h>	/* INFTIM */

#include "timer_queue.h"

static tq_timer_node_t	*pending_delete_chain = NULL;

static void		tq_destroy_timer(tq_t *, tq_timer_node_t *);
static tq_timer_id_t	tq_get_timer_id(tq_t *);
static void		tq_release_timer_id(tq_t *, tq_timer_id_t);

/*
 * tq_create(): creates, initializes and returns a timer queue for use
 *
 *   input: void
 *  output: tq_t *: the new timer queue
 */

tq_t *
tq_create(void)
{
	return (calloc(1, sizeof (tq_t)));
}

/*
 * tq_destroy(): destroys an existing timer queue
 *
 *   input: tq_t *: the timer queue to destroy
 *  output: void
 */

void
tq_destroy(tq_t *tq)
{
	tq_timer_node_t *node, *next_node;

	for (node = tq->head; node != NULL; node = next_node) {
		next_node = node->next;
		tq_destroy_timer(tq, node);
	}

	free(tq);
}

/*
 * tq_insert_timer(): inserts a timer node into a tq's timer list
 *
 *   input: tq_t *: the timer queue
 *	    tq_timer_node_t *: the timer node to insert into the list
 *	    uint32_t: the number of seconds before this timer fires
 *  output: void
 */

static void
tq_insert_timer(tq_t *tq, tq_timer_node_t *node, uint32_t sec)
{
	tq_timer_node_t	*after = NULL;

	/*
	 * find the node to insert this new node "after".  we do this
	 * instead of the more intuitive "insert before" because with
	 * the insert before approach, a null `before' node pointer
	 * is overloaded in meaning (it could be null because there
	 * are no items in the list, or it could be null because this
	 * is the last item on the list, which are very different cases).
	 */

	node->abs_timeout = gethrtime() + ((uint64_t)sec * NANOSEC);

	if (tq->head != NULL && tq->head->abs_timeout < node->abs_timeout)
		for (after = tq->head; after->next != NULL; after = after->next)
			if (after->next->abs_timeout > node->abs_timeout)
				break;

	node->next = after ? after->next : tq->head;
	node->prev = after;
	if (after == NULL)
		tq->head = node;
	else
		after->next = node;

	if (node->next != NULL)
		node->next->prev = node;
}

/*
 * tq_remove_timer(): removes a timer node from the tq's timer list
 *
 *   input: tq_t *: the timer queue
 *	    tq_timer_node_t *: the timer node to remove from the list
 *  output: void
 */

static void
tq_remove_timer(tq_t *tq, tq_timer_node_t *node)
{
	if (node->next != NULL)
		node->next->prev = node->prev;
	if (node->prev != NULL)
		node->prev->next = node->next;
	else
		tq->head = node->next;
}

/*
 * tq_destroy_timer(): destroy a timer node
 *
 *  input: tq_t *: the timer queue the timer node is associated with
 *	   tq_timer_node_t *: the node to free
 * output: void
 */

static void
tq_destroy_timer(tq_t *tq, tq_timer_node_t *node)
{
	tq_release_timer_id(tq, node->timer_id);

	/*
	 * if we're in expire, don't delete the node yet, since it may
	 * still be referencing it (through the expire_next pointers)
	 */

	if (tq->in_expire) {
		node->pending_delete++;
		node->next = pending_delete_chain;
		pending_delete_chain = node;
	} else
		free(node);

}

/*
 * tq_schedule_timer(): creates and inserts a timer in the tq's timer list
 *
 *   input: tq_t *: the timer queue
 *	    uint32_t: the number of seconds before this timer fires
 *	    tq_callback_t *: the function to call when the timer fires
 *	    void *: an argument to pass to the called back function
 *  output: tq_timer_id_t: the new timer's timer id on success, -1 on failure
 */

tq_timer_id_t
tq_schedule_timer(tq_t *tq, uint32_t sec, tq_callback_t *callback, void *arg)
{
	tq_timer_node_t	*node = calloc(1, sizeof (tq_timer_node_t));

	if (node == NULL)
		return (-1);

	node->callback	= callback;
	node->arg	= arg;
	node->timer_id	= tq_get_timer_id(tq);
	if (node->timer_id == -1) {
		free(node);
		return (-1);
	}

	tq_insert_timer(tq, node, sec);

	return (node->timer_id);
}

/*
 * tq_cancel_timer(): cancels a pending timer from a timer queue's timer list
 *
 *   input: tq_t *: the timer queue
 *	    tq_timer_id_t: the timer id returned from tq_schedule_timer
 *	    void **: if non-NULL, a place to return the argument passed to
 *		     tq_schedule_timer
 *  output: int: 1 on success, 0 on failure
 */

int
tq_cancel_timer(tq_t *tq, tq_timer_id_t timer_id, void **arg)
{
	tq_timer_node_t	*node;

	if (timer_id == -1)
		return (0);

	for (node = tq->head; node != NULL; node = node->next) {
		if (node->timer_id == timer_id) {
			if (arg != NULL)
				*arg = node->arg;
			tq_remove_timer(tq, node);
			tq_destroy_timer(tq, node);
			return (1);
		}
	}
	return (0);
}

/*
 * tq_adjust_timer(): adjusts the fire time of a timer in the tq's timer list
 *
 *   input: tq_t *: the timer queue
 *	    tq_timer_id_t: the timer id returned from tq_schedule_timer
 *	    uint32_t: the number of seconds before this timer fires
 *  output: int: 1 on success, 0 on failure
 */

int
tq_adjust_timer(tq_t *tq, tq_timer_id_t timer_id, uint32_t sec)
{
	tq_timer_node_t	*node;

	if (timer_id == -1)
		return (0);

	for (node = tq->head; node != NULL; node = node->next) {
		if (node->timer_id == timer_id) {
			tq_remove_timer(tq, node);
			tq_insert_timer(tq, node, sec);
			return (1);
		}
	}
	return (0);
}

/*
 * tq_earliest_timer(): returns the time until the next timer fires on a tq
 *
 *   input: tq_t *: the timer queue
 *  output: int: the number of milliseconds until the next timer, or INFTIM
 *	         if no timers are pending
 */

int
tq_earliest_timer(tq_t *tq)
{
	int		timeout_interval;

	if (tq->head == NULL)
		return (INFTIM);

	/*
	 * since the timers are ordered in absolute time-to-fire, just
	 * subtract from the head of the list.
	 */

	timeout_interval = (tq->head->abs_timeout - gethrtime()) / 1000000;

	/*
	 * it's possible timeout_interval could be negative if we haven't
	 * gotten a chance to be called in a while.  just return zero and
	 * and pretend it just expired.
	 */

	return ((timeout_interval < 0) ? 0 : timeout_interval);
}

/*
 * tq_expire_timers(): expires all pending timers on a given timer queue
 *
 *   input: tq_t *: the timer queue
 *  output: int: the number of timers expired
 */

int
tq_expire_timers(tq_t *tq)
{
	tq_timer_node_t	*node, *next_node;
	int		n_expired = 0;
	hrtime_t	current_time = gethrtime();

	/*
	 * in_expire is in the tq_t instead of being passed through as
	 * an argument to tq_remove_timer() below since the callback
	 * function may call tq_cancel_timer() itself as well.
	 */

	tq->in_expire++;

	/*
	 * this function builds another linked list of timer nodes
	 * through `expire_next' because the normal linked list
	 * may be changed as a result of callbacks canceling and
	 * scheduling timeouts, and thus can't be trusted.
	 */

	for (node = tq->head; node != NULL; node = node->next)
		node->expire_next = node->next;

	for (node = tq->head; node != NULL; node = node->expire_next) {

		if (node->abs_timeout > current_time)
			break;

		/*
		 * fringe condition: two timers fire at the "same
		 * time" (i.e., they're both scheduled called back in
		 * this loop) and one cancels the other.  in this
		 * case, the timer which has already been "cancelled"
		 * should not be called back.
		 */

		if (node->pending_delete)
			continue;

		/*
		 * we remove the timer before calling back the callback
		 * so that a callback which accidentally tries to cancel
		 * itself (through whatever means) doesn't succeed.
		 */

		n_expired++;
		tq_remove_timer(tq, node);
		tq_destroy_timer(tq, node);
		node->callback(tq, node->arg);
	}

	tq->in_expire--;

	/*
	 * any cancels that took place whilst we were expiring timeouts
	 * ended up on the `pending_delete_chain'.  delete them now
	 * that it's safe.
	 */

	for (node = pending_delete_chain; node != NULL; node = next_node) {
		next_node = node->next;
		free(node);
	}
	pending_delete_chain = NULL;

	return (n_expired);
}

/*
 * tq_get_timer_id(): allocates a timer id from the pool
 *
 *   input: tq_t *: the timer queue
 *  output: tq_timer_id_t: the allocated timer id, or -1 if none available
 */

static tq_timer_id_t
tq_get_timer_id(tq_t *tq)
{
	unsigned int	map_index;
	unsigned char	map_bit;
	boolean_t	have_wrapped = B_FALSE;

	for (; ; tq->next_timer_id++) {

		if (tq->next_timer_id >= TQ_TIMER_ID_MAX) {

			if (have_wrapped)
				return (-1);

			have_wrapped = B_TRUE;
			tq->next_timer_id = 0;
		}

		map_index = tq->next_timer_id / CHAR_BIT;
		map_bit   = tq->next_timer_id % CHAR_BIT;

		if ((tq->timer_id_map[map_index] & (1 << map_bit)) == 0)
			break;
	}

	tq->timer_id_map[map_index] |= (1 << map_bit);
	return (tq->next_timer_id++);
}

/*
 * tq_release_timer_id(): releases a timer id back into the pool
 *
 *   input: tq_t *: the timer queue
 *	    tq_timer_id_t: the timer id to release
 *  output: void
 */

static void
tq_release_timer_id(tq_t *tq, tq_timer_id_t timer_id)
{
	unsigned int	map_index = timer_id / CHAR_BIT;
	unsigned char	map_bit	  = timer_id % CHAR_BIT;

	tq->timer_id_map[map_index] &= ~(1 << map_bit);
}
