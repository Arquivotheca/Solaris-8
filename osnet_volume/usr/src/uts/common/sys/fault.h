/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_FAULT_H
#define	_SYS_FAULT_H

#pragma ident	"@(#)fault.h	1.13	99/08/15 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Fault numbers, analagous to signals.  These correspond to
 * hardware faults.  Setting the appropriate flags in a process's
 * set of traced faults via /proc causes the process to stop each
 * time one of the designated faults occurs so that a debugger can
 * take action.  See proc(4) for details.
 */

	/* fault enumeration must begin with 1 */
#define	FLTILL		1	/* Illegal instruction */
#define	FLTPRIV		2	/* Privileged instruction */
#define	FLTBPT		3	/* Breakpoint instruction */
#define	FLTTRACE	4	/* Trace trap (single-step) */
#define	FLTACCESS	5	/* Memory access (e.g., alignment) */
#define	FLTBOUNDS	6	/* Memory bounds (invalid address) */
#define	FLTIOVF		7	/* Integer overflow */
#define	FLTIZDIV	8	/* Integer zero divide */
#define	FLTFPE		9	/* Floating-point exception */
#define	FLTSTACK	10	/* Irrecoverable stack fault */
#define	FLTPAGE		11	/* Recoverable page fault (no associated sig) */
#define	FLTWATCH	12	/* Watchpoint trap */
#define	FLTCPCOVF	13	/* CPU performance counter overflow */

typedef struct {		/* fault set type */
	unsigned int	word[4];
} fltset_t;

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FAULT_H */
