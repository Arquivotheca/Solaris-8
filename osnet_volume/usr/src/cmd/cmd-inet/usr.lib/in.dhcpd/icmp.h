/*
 * Copyright (c) 1993-1997 by Sun Microsystems, Inc. All rights reserved.
 */

#ifndef	_DHCP_ICMP_H
#define	_DHCP_ICMP_H

#pragma ident	"@(#)icmp.h	1.4	97/04/24 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern int	icmp_echo_register(IF *, PN_REC *, PKT_LIST *);
extern int	icmp_echo_status(IF *, struct in_addr *, int,
    enum dhcp_icmp_flag);

#ifdef	__cplusplus
}
#endif

#endif	/* _DHCP_ICMP_H */
