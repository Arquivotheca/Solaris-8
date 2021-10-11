/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	DEFAULTS_H
#define	DEFAULTS_H

#pragma ident	"@(#)defaults.h	1.1	99/04/09 SMI"

#include <sys/types.h>

/*
 * defaults.[ch] encapsulate the agent's interface to the dhcpagent
 * defaults file.  see defaults.c for documentation on how to use the
 * exported functions.
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * tunable parameters -- keep in the same order as defaults[] in defaults.c
 */

enum {

	DF_RELEASE_ON_SIGTERM,	/* send RELEASE on each if upon SIGTERM */
	DF_IGNORE_FAILED_ARP,	/* what to do if agent can't ARP */
	DF_OFFER_WAIT,		/* how long to wait to collect offers */
	DF_ARP_WAIT,		/* how long to wait for an ARP reply */
	DF_CLIENT_ID,		/* our client id */
	DF_PARAM_REQUEST_LIST	/* our parameter request list */
};

#define	DHCP_AGENT_DEFAULTS	"/etc/default/dhcpagent"

boolean_t	df_get_bool(const char *, unsigned int);
int		df_get_int(const char *, unsigned int);
const char	*df_get_string(const char *, unsigned int);
uchar_t		*df_get_octet(const char *, unsigned int, unsigned int *);

#ifdef	__cplusplus
}
#endif

#endif	/* DEFAULTS_H */
