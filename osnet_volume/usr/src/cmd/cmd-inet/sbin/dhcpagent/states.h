/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	STATES_H
#define	STATES_H

#pragma ident	"@(#)states.h	1.3	99/08/31 SMI"

#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/dhcp.h>

#include "interface.h"
#include "timer_queue.h"
#include "event_handler.h"

/*
 * interfaces for state transition/action functions.  these functions
 * can be found in suitably named .c files, such as inform.c, select.c,
 * renew.c, etc.
 */

#ifdef	__cplusplus
extern "C" {
#endif

void		dhcp_acknak(eh_t *, int, short, eh_event_id_t, void *);
int		dhcp_adopt(void);
int		dhcp_bound(struct ifslist *, PKT_LIST *);
void		dhcp_drop(struct ifslist *);
void		dhcp_expire(tq_t *, void *);
int		dhcp_extending(struct ifslist *);
void		dhcp_inform(struct ifslist *);
void		dhcp_init_reboot(struct ifslist *);
void		dhcp_rebind(tq_t *, void *);
int		dhcp_release(struct ifslist *, char *);
void		dhcp_renew(tq_t *, void *);
void		dhcp_requesting(tq_t *, void *);
void		dhcp_selecting(struct ifslist *);
void		dhcp_start(tq_t *, void *);
void		send_decline(struct ifslist *, char *, struct in_addr *);


#ifdef	__cplusplus
}
#endif

#endif	/* STATES_H */
