/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_DHCPAGENT_UTIL_H
#define	_DHCPAGENT_UTIL_H

#pragma ident	"@(#)dhcpagent_util.h	1.3	99/07/26 SMI"

#include <sys/types.h>
#include <dhcpagent_ipc.h>

/*
 * dhcpagent_util.[ch] provides common utility functions that are
 * useful to dhcpagent consumers.
 */

#ifdef	__cplusplus
extern "C" {
#endif

extern const char	*dhcp_state_to_string(DHCPSTATE);
extern dhcp_ipc_type_t  dhcp_string_to_request(const char *);
extern int		dhcp_start_agent(int);

#ifdef	__cplusplus
}
#endif

#endif	/* _DHCPAGENT_UTIL_H */
