/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*		All Rights Reserved				*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_FP_H
#define	_SYS_FP_H

#pragma ident	"@(#)fp.h	1.10	99/05/04 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 80287/80387 floating point processor definitions
 */

/*
 * values that go into fp_kind
 */
#define	FP_NO	0	/* no fp chip, no emulator (no fp support)	*/
#define	FP_SW	1	/* no fp chip, using software emulator		*/
#define	FP_HW	2	/* chip present bit				*/
#define	FP_287	2	/* 80287 chip present				*/
#define	FP_387	3	/* 80387 chip present				*/
#define	FP_487	6	/* 80487 chip present				*/
#define	FP_486	6	/* 80486 chip present				*/

/*
 * masks for 80387 control word
 */
#define	FPINV	0x00000001	/* invalid operation			*/
#define	FPDNO	0x00000002	/* denormalized operand			*/
#define	FPZDIV	0x00000004	/* zero divide				*/
#define	FPOVR	0x00000008	/* overflow				*/
#define	FPUNR	0x00000010	/* underflow				*/
#define	FPPRE	0x00000020	/* precision				*/
#define	FPPC	0x00000300	/* precision control			*/
#define	FPRC	0x00000C00	/* rounding control			*/
#define	FPIC	0x00001000	/* infinity control			*/
#define	WFPDE	0x00000080	/* data chain exception			*/

/*
 * precision, rounding, and infinity options in control word
 */
#define	FPSIG24 0x00000000	/* 24-bit significand precision (short) */
#define	FPSIG53 0x00000200	/* 53-bit significand precision (long)	*/
#define	FPSIG64 0x00000300	/* 64-bit significand precision (temp)	*/
#define	FPRTN	0x00000000	/* round to nearest or even		*/
#define	FPRD	0x00000400	/* round down				*/
#define	FPRU	0x00000800	/* round up				*/
#define	FPCHOP	0x00000C00	/* chop (truncate toward zero)		*/
#define	FPP	0x00000000	/* projective infinity			*/
#define	FPA	0x00001000	/* affine infinity			*/
#define	WFPB17	0x00020000	/* bit 17				*/
#define	WFPB24	0x01000000	/* bit 24				*/

/*
 * masks for 80387 status word
 */
#define	FPS_IE	0x00000001	/* invalid operation			*/
#define	FPS_DE	0x00000002	/* denormalized operand			*/
#define	FPS_ZE	0x00000004	/* zero devide				*/
#define	FPS_OE	0x00000008	/* overflow				*/
#define	FPS_UE	0x00000010	/* underflow				*/
#define	FPS_PE	0x00000020	/* precision				*/
#define	FPS_SF	0x00000040	/* stack fault				*/
#define	FPS_ES	0x00000080	/* error summary bit			*/
#define	FPS_C0	0x00000100	/* C0 bit				*/
#define	FPS_C1	0x00000200	/* C1 bit				*/
#define	FPS_C2	0x00000400	/* C2 bit				*/
#define	FPS_TOP	0x00003800	/* top of stack pointer			*/
#define	FPS_C3	0x00004000	/* C3 bit				*/
#define	FPS_B	0x00008000	/* busy bit				*/

/* Initial value of FPU control word as per ABI document */
#define	FPU_CW_INIT	0x1332

extern int fp_kind;		/* kind of fp support			*/
extern int fpu_exists;		/* FPU hw exists			*/

#ifdef _KERNEL

extern void fpu_probe(void);
extern void fpsave(struct fpu *);
extern void fpksave(struct fpu *);
extern void fprestore(struct fpu *);
extern void fpenable(void);
extern void fpdisable(void);
extern void fpinit(void);
extern int fperr_reset(void);
extern int fpnoextflt(struct regs *rp);
extern int fpextovrflt(struct regs *rp);
extern int fpexterrflt(struct regs *rp);

#endif	/* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_FP_H */
