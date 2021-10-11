/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

/*
 * Miscellaneous internetwork
 * definitions for kernel.
 */

#ifndef	_NETINET_IN_SYSTM_H
#define	_NETINET_IN_SYSTM_H

#pragma ident	"@(#)in_systm.h	1.5	98/01/06 SMI"
/* in_systm.h 1.8 88/08/19 SMI; from UCB 7.1 6/5/86	*/

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Network types.
 *
 * Internally the system keeps counters in the headers with the bytes
 * swapped so that VAX instructions will work on them.  It reverses
 * the bytes before transmission at each protocol level.  The n_ types
 * represent the types with the bytes in ``high-ender'' order.
 */
typedef ushort_t n_short;		/* short as received from the net */
typedef ulong_t	n_long;			/* long as received from the net */
typedef	ulong_t	n_time;			/* ms since 00:00 GMT, byte rev */

#ifdef	__cplusplus
}
#endif

#endif	/* _NETINET_IN_SYSTM_H */
