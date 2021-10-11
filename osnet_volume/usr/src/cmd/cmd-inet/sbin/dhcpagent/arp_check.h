/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	ARP_CHECK_H
#define	ARP_CHECK_H

#pragma ident	"@(#)arp_check.h	1.1	99/04/09 SMI"

#include <sys/types.h>
#include <netinet/in.h>

#include "interface.h"

/*
 * arp_check.[ch] provide an interface for checking whether a given IP
 * address is currently in use.  see arp_check.c for documentation on
 * how to use the exported function.
 */

#ifdef	__cplusplus
extern "C" {
#endif

int		arp_check(struct ifslist *, in_addr_t, in_addr_t, uchar_t *,
		    uint32_t, uint32_t);

#ifdef	__cplusplus
}
#endif

#endif	/* ARP_CHECK_H */
