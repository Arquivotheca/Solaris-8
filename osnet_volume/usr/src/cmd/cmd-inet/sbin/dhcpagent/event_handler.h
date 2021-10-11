/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	EVENT_HANDLER_H
#define	EVENT_HANDLER_H

#pragma ident	"@(#)event_handler.h	1.2	99/09/22 SMI"

#include <sys/types.h>
#include <sys/poll.h>
#include <signal.h>

#include "timer_queue.h"

/*
 * an event handler is an object-oriented "wrapper" for select(3C) /
 * poll(2), aimed to make the event demultiplexing system calls easier
 * to use and provide a generic reusable component.  instead of
 * applications directly using select(3C) / poll(2), they register
 * events that should be received with the event handler, providing a
 * callback function to call when the event occurs.  they then call
 * eh_handle_events() to wait and callback the registered functions
 * when events occur.  also called a `reactor'.
 */

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct event_handler eh_t;

/*
 * an eh_event_id_t refers to a given event.  its value should not be
 * interpreted by the interface consumer.  it is a signed arithmetic
 * type, and no valid eh_event_id_t has the value `-1'.
 */

typedef int eh_event_id_t;

/*
 * an eh_callback_t is a function that is called back in response to
 * an event occurring.  it may then carry out any work necessary in
 * response to the event.  it receives the file descriptor upon which
 * the event occurred, a bit array of events that occurred (the same
 * array used as the revents by poll(2)), and its context through the
 * `void *' that was originally passed into eh_register_event().
 *
 * NOTE: the same descriptor may not be registered multiple times for
 * different callbacks.  if this behavior is desired, either use dup(2)
 * to get a unique descriptor, or demultiplex in the callback function
 * based on the events.
 */

typedef void	eh_callback_t(eh_t *, int, short, eh_event_id_t, void *);
typedef void	eh_sighandler_t(eh_t *, int, void *);

eh_t		*eh_create(void);
void		eh_destroy(eh_t *);
eh_event_id_t	eh_register_event(eh_t *, int, short, eh_callback_t *, void *);
int		eh_unregister_event(eh_t *, eh_event_id_t, void **);
int		eh_handle_events(eh_t *, tq_t *);
void		eh_stop_handling_events(eh_t *, unsigned int);
int		eh_register_signal(eh_t *, int, eh_sighandler_t *, void *);
int		eh_unregister_signal(eh_t *, int, void **);

/*
 * the remainder of this file contains implementation-specific
 * artifacts which may change.  an `eh_t' is an incomplete type as far
 * as the consumer of event handlers is concerned.
 */

typedef struct event_node {

	eh_callback_t	*callback;	/* callback to call */
	void		*arg;		/* argument to pass to the callback */

} eh_event_node_t;

typedef struct eh_sig_info  {

	boolean_t	pending;	/* signal is currently pending */
	eh_sighandler_t	*handler;	/* handler for a given signal */
	void		*data; 		/* data to pass back to the handler */

} eh_sig_info_t;

struct event_handler {

	struct pollfd	*pollfds;	/* array of pollfds */
	eh_event_node_t	*events;	/* corresponding pollfd info */
	unsigned int	num_fds;	/* number of pollfds/events */
	boolean_t	stop;		/* true when done */
	unsigned int	reason;		/* if stop is true, reason */
	sigset_t	sig_regset;	/* registered signal set */
	eh_sig_info_t	sig_info[NSIG];	/* signal handler information */
};

#ifdef	__cplusplus
}
#endif

#endif	/* EVENT_HANDLER_H */
