/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	ASYNC_H
#define	ASYNC_H

#pragma ident	"@(#)async.h	1.1	99/04/09 SMI"

#include <sys/types.h>
#include <dhcpagent_ipc.h>

#include "timer_queue.h"

/*
 * async.[ch] comprise the interface used to handle asynchronous DHCP
 * commands.  see ipc_event() in agent.c for more documentation on
 * the treatment of asynchronous DHCP commands.  see async.c for
 * documentation on how to use the exported functions.
 */

#ifdef	__cplusplus
extern "C" {
#endif

struct ifslist;					/* forward declaration */

struct async_action {

	dhcp_ipc_type_t		as_cmd;		/* command/action in progress */
	tq_timer_id_t		as_tid;		/* async timer id */
	boolean_t		as_user;	/* user-generated async cmd */
};

#define	DHCP_ASYNC_WAIT		60		/* seconds */

boolean_t	async_pending(struct ifslist *);
int		async_start(struct ifslist *, int, boolean_t);
void		async_finish(struct ifslist *);
int		async_cancel(struct ifslist *);

#ifdef	__cplusplus
}
#endif

#endif	/* ASYNC_H */
