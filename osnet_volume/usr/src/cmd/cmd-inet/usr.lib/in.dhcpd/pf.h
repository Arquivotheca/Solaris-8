/*
 * Copyright (c) 1993 by Sun Microsystems, Inc. All rights reserved.
 */

#ifndef	_DHCP_PF_H
#define	_DHCP_PF_H

#pragma ident	"@(#)pf.h	1.4	95/12/04 SMI"

#include <sys/pfmod.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern struct packetfilt	dhcppf;
extern void			initialize_pf(void);

#ifdef	__cplusplus
}
#endif

#endif	/* _DHCP_PF_H */
