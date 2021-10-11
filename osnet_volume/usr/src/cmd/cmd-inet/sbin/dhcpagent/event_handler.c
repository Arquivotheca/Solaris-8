/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)event_handler.c	1.2	99/09/22 SMI"

#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stropts.h>	/* INFTIM */

#include "event_handler.h"

static int	eh_grow_fds(eh_t *, int);

/*
 * signal_to_eh[] is pretty much useless, since the event handler is
 * really a singleton (we pass eh_t *'s around to maintain an
 * abstraction, not to allow multiple event handlers to exist).  we
 * need some way to get back our event handler in eh_post_signal(),
 * and since the signal model is too lame to provide opaque pointers,
 * we have to resort to global variables.
 */

static eh_t *signal_to_eh[NSIG];

/*
 * eh_create(): creates, initializes, and returns an event handler for use
 *
 *   input: void
 *  output: eh_t *: the new event handler
 */

eh_t *
eh_create(void)
{
	eh_t	*eh = malloc(sizeof (eh_t));
	int	sig;

	eh->pollfds	= NULL;
	eh->events	= NULL;
	eh->num_fds	= 0;
	eh->stop	= B_FALSE;
	eh->reason	= 0;

	(void) sigemptyset(&eh->sig_regset);
	for (sig = 0; sig < NSIG; sig++) {
		eh->sig_info[sig].pending = B_FALSE;
		eh->sig_info[sig].handler = NULL;
		eh->sig_info[sig].data = NULL;
	}

	return (eh);
}

/*
 * eh_destroy(): destroys an existing event handler
 *
 *   input: eh_t *: the event handler to destroy
 *  output: void
 *   notes: it is assumed all events related to this eh have been unregistered
 *          prior to calling eh_destroy()
 */

void
eh_destroy(eh_t *eh)
{
	int	sig;

	for (sig = 0; sig < NSIG; sig++)
		if (signal_to_eh[sig] == eh)
			(void) eh_unregister_signal(eh, sig, NULL);

	free(eh->pollfds);
	free(eh->events);
	free(eh);
}

/*
 * eh_stop_handling_events(): informs the event handler to stop handling events
 *
 *   input: eh_t *: the event handler to stop.
 *	    unsigned int: the (user-defined) reason why
 *  output: void
 *   notes: the event handler in question must be in eh_handle_events()
 */


void
eh_stop_handling_events(eh_t *eh, unsigned int reason)
{
	eh->stop   = B_TRUE;
	eh->reason = reason;
}

/*
 * eh_grow_fds(): grows the internal file descriptor set used by the event
 *		  handler
 *
 *   input: eh_t *: the event handler whose descriptor set needs to be grown
 *          int: the new total number of descriptors needed in the set
 *  output: int: zero on failure, success otherwise
 */

static int
eh_grow_fds(eh_t *eh, int total_fds)
{
	unsigned int	i;
	struct pollfd	*new_pollfds;
	eh_event_node_t	*new_events;

	if (total_fds <= eh->num_fds)
		return (1);

	new_pollfds = realloc(eh->pollfds, total_fds * sizeof (struct pollfd));
	if (new_pollfds == NULL)
		return (0);

	eh->pollfds = new_pollfds;

	new_events = realloc(eh->events, total_fds * sizeof (eh_event_node_t));
	if (new_events == NULL) {

		/*
		 * yow.  one realloc failed, but the other succeeded.
		 * we will just leave the descriptor size at the
		 * original size.  if the caller tries again, then the
		 * first realloc() will do nothing since the requested
		 * number of descriptors is already allocated.
		 */

		return (0);
	}

	for (i = eh->num_fds; i < total_fds; i++)
		eh->pollfds[i].fd = -1;

	eh->events  = new_events;
	eh->num_fds = total_fds;
	return (1);
}

/*
 * when increasing the file descriptor set size, how much to increase by:
 */

#define	EH_FD_SLACK	10

/*
 * eh_register_event(): adds an event to the set managed by an event handler
 *
 *   input: eh_t *: the event handler to add the event to
 *          int: the descriptor on which to listen for events.  must be
 *		 a descriptor which has not yet been registered.
 *          short: the events to listen for on that descriptor
 *          eh_callback_t: the callback to execute when the event happens
 *          void *: the argument to pass to the callback function
 *  output: eh_event_id_t: -1 on failure, the new event id otherwise
 */

eh_event_id_t
eh_register_event(eh_t *eh, int fd, short events, eh_callback_t *callback,
    void *arg)
{
	if (eh->num_fds <= fd)
		if (eh_grow_fds(eh, fd + EH_FD_SLACK) == 0)
			return (-1);

	/*
	 * the current implementation uses the file descriptor itself
	 * as the eh_event_id_t, since we know the kernel's gonna be
	 * pretty smart about managing file descriptors and we know
	 * that they're per-process unique.  however, it does mean
	 * that the same descriptor cannot be registered multiple
	 * times for different callbacks depending on its events.  if
	 * this behavior is desired, either use dup(2) to get a unique
	 * descriptor, or demultiplex in the callback function based
	 * on `events'.
	 */

	if (eh->pollfds[fd].fd != -1)
		return (-1);

	eh->pollfds[fd].fd 	= fd;
	eh->pollfds[fd].events	= events;
	eh->events[fd].callback	= callback;
	eh->events[fd].arg	= arg;

	return (fd);
}

/*
 * eh_unregister_event(): removes an event from the set managed by an event
 *			  handler
 *
 *   input: eh_t *: the event handler to remove the event from
 *          eh_event_id_t: the event to remove (from eh_register_event())
 *          void **: if non-NULL, will be set to point to the argument passed
 *                   into eh_register_event()
 *  output: int: zero on failure, success otherwise
 */

int
eh_unregister_event(eh_t *eh, eh_event_id_t event_id, void **arg)
{
	if (event_id < 0 || event_id >= eh->num_fds ||
	    eh->pollfds[event_id].fd == -1)
		return (0);

	/*
	 * fringe condition: in case this event was about to be called
	 * back in eh_handle_events(), zero revents to prevent it.
	 * (having an unregistered event get called back could be
	 * disastrous depending on if `arg' is reference counted).
	 */

	eh->pollfds[event_id].revents = 0;
	eh->pollfds[event_id].fd = -1;
	if (arg != NULL)
		*arg = eh->events[event_id].arg;

	return (1);
}

/*
 * eh_handle_events(): begins handling events on an event handler
 *
 *   input: eh_t *: the event handler to begin event handling on
 *          tq_t *: a timer queue of timers to process while handling events
 *                  (see timer_queue.h for details)
 *  output: int: the reason why we stopped, -1 if due to internal failure
 */

int
eh_handle_events(eh_t *eh, tq_t *tq)
{
	int		n_lit, timeout, sig, saved_errno;
	unsigned int	i;
	sigset_t	oset;

	eh->stop = B_FALSE;
	do {
		timeout = tq ? tq_earliest_timer(tq) : INFTIM;

		/*
		 * we only unblock registered signals around poll(); this
		 * way other parts of the code don't have to worry about
		 * restarting "non-restartable" system calls and so forth.
		 */

		(void) sigprocmask(SIG_UNBLOCK, &eh->sig_regset, &oset);
		n_lit = poll(eh->pollfds, eh->num_fds, timeout);
		saved_errno = errno;
		(void) sigprocmask(SIG_SETMASK, &oset, NULL);

		switch (n_lit) {

		case -1:
			if (saved_errno != EINTR)
				return (-1);

			for (sig = 0; sig < NSIG; sig++) {
				if (eh->sig_info[sig].pending) {
					eh->sig_info[sig].pending = B_FALSE;
					eh->sig_info[sig].handler(eh, sig,
					    eh->sig_info[sig].data);
				}
			}
			continue;

		case  0:
			/*
			 * timeout occurred.  we must have a valid tq pointer
			 * since that's the only way a timeout can happen.
			 */

			(void) tq_expire_timers(tq);
			continue;

		default:
			break;
		}

		/* file descriptors are lit; call 'em back */

		for (i = 0; i < eh->num_fds && n_lit > 0; i++) {

			if (eh->pollfds[i].revents == 0)
				continue;

			n_lit--;

			/*
			 * turn off any descriptors that have gone
			 * bad.  shouldn't happen, but...
			 */

			if (eh->pollfds[i].revents & (POLLNVAL|POLLERR)) {
				/* TODO: issue a warning here - but how? */
				(void) eh_unregister_event(eh, i, NULL);
				continue;
			}

			eh->events[i].callback(eh, i, eh->pollfds[i].revents,
			    i, eh->events[i].arg);
		}

	} while (eh->stop == B_FALSE);

	return (eh->reason);
}

/*
 * eh_post_signal(): posts a signal for later consumption in eh_handle_events()
 *
 *   input: int: the signal that's been received
 *  output: void
 */

static void
eh_post_signal(int sig)
{
	if (signal_to_eh[sig] != NULL)
		signal_to_eh[sig]->sig_info[sig].pending = B_TRUE;
}

/*
 * eh_register_signal(): registers a signal handler with an event handler
 *
 *   input: eh_t *: the event handler to register the signal handler with
 *	    int: the signal to register
 *	    eh_sighandler_t *: the signal handler to call back
 *	    void *: the argument to pass to the signal handler function
 *   output: int: zero on failure, success otherwise
 */

int
eh_register_signal(eh_t *eh, int sig, eh_sighandler_t *handler, void *data)
{
	struct sigaction	act;

	if (sig < 0 || sig >= NSIG || signal_to_eh[sig] != NULL)
		return (0);

	act.sa_flags = 0;
	act.sa_handler = &eh_post_signal;
	(void) sigemptyset(&act.sa_mask);
	(void) sigaddset(&act.sa_mask, sig); /* used for sigprocmask() */

	if (sigaction(sig, &act, NULL) == -1)
		return (0);

	(void) sigprocmask(SIG_BLOCK, &act.sa_mask, NULL);

	eh->sig_info[sig].data = data;
	eh->sig_info[sig].handler = handler;
	signal_to_eh[sig] = eh;

	(void) sigaddset(&eh->sig_regset, sig);
	return (0);
}

/*
 * eh_unregister_signal(): unregisters a signal handler from an event handler
 *
 *   input: eh_t *: the event handler to unregister the signal handler from
 *	    int: the signal to unregister
 *	    void **: if non-NULL, will be set to point to the argument passed
 *		     into eh_register_signal()
 *  output: int: zero on failure, success otherwise
 */

int
eh_unregister_signal(eh_t *eh, int sig, void **datap)
{
	sigset_t	set;

	if (sig < 0 || sig >= NSIG || signal_to_eh[sig] != eh)
		return (0);

	if (signal(sig, SIG_DFL) == SIG_ERR)
		return (0);

	if (datap != NULL)
		*datap = eh->sig_info[sig].data;

	(void) sigemptyset(&set);
	(void) sigaddset(&set, sig);
	(void) sigprocmask(SIG_UNBLOCK, &set, NULL);

	eh->sig_info[sig].data = NULL;
	eh->sig_info[sig].handler = NULL;
	eh->sig_info[sig].pending = B_FALSE;
	signal_to_eh[sig] = NULL;

	(void) sigdelset(&eh->sig_regset, sig);
	return (1);
}
