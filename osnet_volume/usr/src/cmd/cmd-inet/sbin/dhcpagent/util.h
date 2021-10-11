/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	UTIL_H
#define	UTIL_H

#pragma ident	"@(#)util.h	1.8	99/09/21 SMI"

#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/dhcp.h>
#include <dhcpagent_ipc.h>

#include "timer_queue.h"

/*
 * general utility functions which have no better home.  see util.c
 * for documentation on how to use the exported functions.
 */

#ifdef	__cplusplus
extern "C" {
#endif

struct ifslist;				/* forward declaration */

typedef int64_t monosec_t;		/* see README for details */

/* conversion functions */
const char	*pkt_type_to_string(uchar_t);
const char	*monosec_to_string(monosec_t);
time_t		monosec_to_time(monosec_t);
uchar_t		dlpi_to_arp(uchar_t);

/* shutdown handlers */
void		graceful_shutdown(int);
void		inactivity_shutdown(tq_t *, void *);

/* acknak handlers */
int		register_acknak(struct ifslist *);
int		unregister_acknak(struct ifslist *);

/* ipc functions */
void		send_error_reply(dhcp_ipc_request_t *, int, int *);
void		send_ok_reply(dhcp_ipc_request_t *, int *);
void		send_data_reply(dhcp_ipc_request_t *, int *, int,
		    dhcp_data_type_t, void *, size_t);

/* miscellaneous */
int		add_default_route(int, struct in_addr *);
int		del_default_route(int, struct in_addr *);
int		daemonize(void);
monosec_t	monosec(void);
void		print_server_msg(struct ifslist *, DHCP_OPT *);
int		bind_sock(int, in_port_t, in_addr_t);

#ifdef	__cplusplus
}
#endif

#endif	/* UTIL_H */
