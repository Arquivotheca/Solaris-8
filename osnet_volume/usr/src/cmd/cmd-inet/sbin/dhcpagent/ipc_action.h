/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	IPC_ACTION_H
#define	IPC_ACTION_H

#pragma ident	"@(#)ipc_action.h	1.2	99/07/08 SMI"

#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/dhcp.h>
#include <dhcpagent_ipc.h>

#include "agent.h"
#include "timer_queue.h"

/*
 * ipc_action.[ch] make up the interface used to control the current
 * pending interprocess communication transaction taking place.  see
 * ipc_action.c for documentation on how to use the exported functions.
 */

#ifdef	__cplusplus
extern "C" {
#endif

struct ifslist;					/* forward declaration */

void		ipc_action_init(struct ifslist *);
int		ipc_action_start(struct ifslist *, dhcp_ipc_request_t *, int);
void		ipc_action_finish(struct ifslist *, int);
boolean_t	ipc_action_pending(struct ifslist *);
void		ipc_action_cancel_timer(struct ifslist *);


struct ipc_action {

	dhcp_ipc_type_t		ia_cmd;		/* command/action requested  */
	int			ia_fd;		/* ipc channel descriptor */
	tq_timer_id_t		ia_tid;		/* ipc timer id */
	dhcp_ipc_request_t	*ia_request;	/* ipc request pointer */
};

#ifdef	__cplusplus
}
#endif

#endif	/* IPC_ACTION_H */
