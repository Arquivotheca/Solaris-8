/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _IA32_SYS_PSW_H
#define	_IA32_SYS_PSW_H

#pragma ident	"@(#)psw.h	1.2	99/05/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _ASM

/* Flags Register */

typedef struct flags {
	uint_t	fl_cf	:  1,		/* carry/borrow */
			:  1,		/* reserved */
		fl_pf	:  1,		/* parity */
			:  1,		/* reserved */
		fl_af	:  1,		/* carry/borrow */
			:  1,		/* reserved */
		fl_zf	:  1,		/* zero */
		fl_sf	:  1,		/* sign */
		fl_tf	:  1,		/* trace */
		fl_if	:  1,		/* interrupt enable */
		fl_df	:  1,		/* direction */
		fl_of	:  1,		/* overflow */
		fl_iopl :  2,		/* I/O privilege level */
		fl_nt	:  1,		/* nested task */
			:  1,		/* reserved */
		fl_rf	:  1,		/* reset */
		fl_vm	:  1,		/* virtual 86 mode */
		fl_res	: 14;		/* reserved */
} flags_t;

#endif		/* !_ASM */

#define	PS_C		0x0001		/* carry bit			*/
#define	PS_P		0x0004		/* parity bit			*/
#define	PS_AC		0x0010		/* auxiliary carry bit		*/
#define	PS_Z		0x0040		/* zero bit			*/
#define	PS_N		0x0080		/* negative bit			*/
#define	PS_T		0x0100		/* trace enable bit		*/
#define	PS_IE		0x0200		/* interrupt enable bit		*/
#define	PS_D		0x0400		/* direction bit		*/
#define	PS_V		0x0800		/* overflow bit			*/
#define	PS_IOPL		0x3000		/* I/O privilege level		*/
#define	PS_NT		0x4000		/* nested task flag		*/
#define	PS_RF		0x10000		/* Reset flag			*/
#define	PS_VM		0x20000		/* Virtual 86 mode flag		*/
#define	PS_ACHK		0x40000		/* Alignment check enable (486) */

#define	PS_ICC		(PS_C|PS_AC|PS_Z|PS_N)	   /* integer condition codes */

#define	PSL_USER	0x202		/* initial user EFLAGS */

/* user variable PS bits */
#define	PSL_USERMASK	(PS_ICC|PS_D|PS_T|PS_V|PS_P|PS_ACHK|PS_NT)

#ifndef _ASM
typedef int	psw_t;
#endif

#include <sys/segment.h>			/* selector definitions */

#define	USERMODE(cs)	((uint_t)((cs)&CPL_MASK) != 0)

#include <sys/spl.h>

#ifdef	__cplusplus
}
#endif

#endif	/* _IA32_SYS_PSW_H */
