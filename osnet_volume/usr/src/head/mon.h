/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _MON_H
#define	_MON_H

#pragma ident	"@(#)mon.h	1.13	97/12/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct hdr {
	char	*lpc;
	char	*hpc;
	size_t	nfns;
};

struct cnt {
	char	*fnpc;
	long	mcnt;
};

typedef unsigned short WORD;

#define	MON_OUT	"mon.out"
#define	MPROGS0	(150 * sizeof (WORD))	/* 300 for pdp11, 600 for 32-bits */
#define	MSCALE0	4

#ifndef NULL
#if defined(_LP64) && !defined(__cplusplus)
#define	NULL	0L
#else
#define	NULL	0
#endif
#endif

#if defined(__STDC__)
extern void monitor(int (*)(void), int (*)(void), WORD *, size_t, size_t);
#else
extern void monitor();
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _MON_H */
