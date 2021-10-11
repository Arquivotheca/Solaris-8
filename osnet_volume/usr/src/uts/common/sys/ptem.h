/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_PTEM_H
#define	_SYS_PTEM_H

#pragma ident	"@(#)ptem.h	1.15	98/05/13 SMI"	/* SVr4.0 11.7	*/

#include <sys/termios.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The ptem data structure used to define the global data
 * for the psuedo terminal emulation streams module
 */
struct ptem {
	tcflag_t cflags;	/* copy of c_cflags */
	mblk_t *dack_ptr;	/* preallocated mblk used to ACK disconnect */
	queue_t *q_ptr;		/* ptem read queue */
	struct winsize wsz;	/* struct to hold the windowing info. */
	unsigned short state;	/* state of ptem entry: see below */
};

/*
 * state flags
 */
#define	REMOTEMODE	0x1	/* Pty in remote mode */
#define	OFLOW_CTL	0x2	/* Outflow control on */
#define	IS_PTSTTY	0x4	/* is x/open terminal */

/*
 * Constants used to distinguish between a common function invoked
 * from the read or write side put procedures
 */
#define	RDSIDE	1
#define	WRSIDE	2

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PTEM_H */
