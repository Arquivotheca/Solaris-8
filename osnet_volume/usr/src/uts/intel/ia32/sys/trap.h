/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any		*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _IA32_SYS_TRAP_H
#define	_IA32_SYS_TRAP_H

#pragma ident	"@(#)trap.h	1.2	99/06/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Trap type values
 */

#define	T_ZERODIV	0x0		/* divide by 0 error		*/
#define	T_SGLSTP	0x1		/* single step			*/
#define	T_NMIFLT	0x2		/* NMI				*/
#define	T_BPTFLT	0x3		/* breakpoint fault		*/
#define	T_OVFLW		0x4		/* INTO overflow fault		*/
#define	T_BOUNDFLT	0x5		/* BOUND instruction fault	*/
#define	T_ILLINST	0x6		/* invalid opcode fault		*/
#define	T_NOEXTFLT	0x7		/* extension not available fault */
#define	T_DBLFLT	0x8		/* double fault			*/
#define	T_EXTOVRFLT	0x9		/* extension overrun fault	*/
#define	T_TSSFLT	0xa		/* invalid TSS fault		*/
#define	T_SEGFLT	0xb		/* segment not present fault	*/
#define	T_STKFLT	0xc		/* stack fault			*/
#define	T_GPFLT		0xd		/* general protection fault	*/
#define	T_PGFLT		0xe		/* page fault			*/
#define	T_EXTERRFLT	0x10		/* extension error fault	*/
#define	T_ALIGNMENT	0x11		/* alignment check error (486 only) */
#define	T_MCE		0x12		/* machine check exception (P6 only) */
#define	T_ENDPERR	0x21		/* emulated extension error flt	*/
#define	T_ENOEXTFLT	0x20		/* emulated ext not present	*/
#define	T_FASTTRAP	0xd2		/* fast system call		*/
#define	T_SOFTINT	0x50fd		/* pseudo softint trap type	*/

/*
 * Pseudo traps.
 * XXX - check?
 */
#define	T_INTERRUPT		0x100
#define	T_FAULT			0x200
#define	T_AST			0x400
#define	T_SYSCALL		0x180		/* XXX - check? */


/*
 *  Values of error code on stack in case of page fault
 */

#define	PF_ERR_MASK	0x01	/* Mask for error bit */
#define	PF_ERR_PAGE	0	/* page not present */
#define	PF_ERR_PROT	1	/* protection error */
#define	PF_ERR_WRITE	2	/* fault caused by write (else read) */
#define	PF_ERR_USER	4	/* processor was in user mode */
				/*	(else supervisor) */

/*
 *  Definitions for fast system call subfunctions
 */
#define	T_FNULL		0	/* Null trap for testing		*/
#define	T_FGETFP	1	/* Get emulated FP context		*/
#define	T_FSETFP	2	/* Set emulated FP context		*/
#define	T_GETHRTIME	3	/* Get high resolution time		*/
#define	T_GETHRVTIME	4	/* Get high resolution virtual time	*/
#define	T_GETHRESTIME	5	/* Get high resolution time		*/

#define	T_LASTFAST	5	/* Last valid subfunction		*/

#ifdef	__cplusplus
}
#endif

#endif	/* _IA32_SYS_TRAP_H */
