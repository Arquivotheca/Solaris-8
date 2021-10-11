/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_STRMDEP_H
#define	_SYS_STRMDEP_H

#pragma ident	"@(#)strmdep.h	1.10	98/01/06 SMI"	/* SVr4.0 1.3	*/

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This file contains all machine-dependent declarations
 * in STREAMS.
 */

/*
 * Copy data from one data buffer to another.
 * The addresses must be word aligned - if not, use bcopy!
 */
#define	strbcpy(s, d, c)	bcopy(s, d, c)

/*
 * save the address of the calling function on the 3b2 to
 * enable tracking of who is allocating message blocks
 */
#define	saveaddr(funcp)

/*
 * macro to check pointer alignment
 * (true if alignment is sufficient for worst case)
 */
#define	str_aligned(X)	(((ulong_t)(X) & (sizeof (long) - 1)) == 0)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_STRMDEP_H */
