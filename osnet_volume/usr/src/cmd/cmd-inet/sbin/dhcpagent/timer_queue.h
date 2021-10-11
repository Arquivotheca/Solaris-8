/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	TIMER_QUEUE_H
#define	TIMER_QUEUE_H

#pragma ident	"@(#)timer_queue.h	1.3	99/09/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <limits.h>

/*
 * timer queues are a facility for managing timeouts in unix.  in the
 * event driven model, unix provides us with poll(2)/select(3C), which
 * allow us to coordinate waiting on multiple descriptors with an
 * optional timeout.  however, often (as is the case with the DHCP
 * agent), we want to manage multiple independent timeouts (say, one
 * for waiting for an OFFER to come back from a server in response to
 * a DISCOVER sent out on one interface, and another for waiting for
 * the T1 time on another interface).  timer queues allow us to do
 * this in the event-driven model.
 *
 * note that timer queues do not in and of themselves provide the
 * event driven model (for instance, there is no handle_events()
 * routine).  they merely provide the hooks to support multiple
 * independent timeouts.  this is done for both simplicity and
 * applicability (for instance, while one approach would be to use
 * this timer queue with poll(2), another one would be to use SIGALRM
 * to wake up periodically, and then process all the expired timers.)
 */

typedef struct timer_queue tq_t;

/*
 * a tq_timer_id_t refers to a given timer.  its value should not be
 * interpreted by the interface consumer.  it is a signed arithmetic
 * type, and no valid tq_timer_id_t has the value `-1'.
 */

typedef int tq_timer_id_t;

#define	TQ_TIMER_ID_MAX	1024	/* max number of concurrent timers */

/*
 * a tq_callback_t is a function that is called back in response to a
 * timer expiring.  it may then carry out any necessary work,
 * including rescheduling itself for callback or scheduling /
 * cancelling other timers.  the `void *' argument is the same value
 * that was passed into tq_schedule_timer(), and if it is dynamically
 * allocated, it is the callback's responsibility to know that, and to
 * free it.
 */

typedef void	tq_callback_t(tq_t *, void *);

tq_t		*tq_create(void);
void		tq_destroy(tq_t *);
tq_timer_id_t	tq_schedule_timer(tq_t *, uint32_t, tq_callback_t *, void *);
int		tq_adjust_timer(tq_t *, tq_timer_id_t, uint32_t);
int		tq_cancel_timer(tq_t *, tq_timer_id_t, void **);
int		tq_expire_timers(tq_t *);
int		tq_earliest_timer(tq_t *);

/*
 * the remainder of this file contains implementation-specific
 * artifacts which may change.  a `tq_t' is an incomplete type as far
 * as the consumer of timer queues is concerned.
 */

typedef struct timer_node {

	struct timer_node *prev;
	struct timer_node *next;
	struct timer_node *expire_next;
	hrtime_t	  abs_timeout;
	tq_timer_id_t	  timer_id;
	tq_callback_t	  *callback;
	void		  *arg;
	int		  pending_delete;

} tq_timer_node_t;

struct timer_queue {

	tq_timer_id_t	 next_timer_id;
	tq_timer_node_t	 *head;		/* in order of time-to-fire */
	int		 in_expire;	/* nonzero if in the expire function */
	unsigned char	 timer_id_map[(TQ_TIMER_ID_MAX + CHAR_BIT) / CHAR_BIT];
};

#ifdef	__cplusplus
}
#endif

#endif	/* TIMER_QUEUE_H */
