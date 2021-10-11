/*
 * Copyright (c) 1986-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_PRIVREGS_H
#define	_SYS_PRIVREGS_H

#pragma ident	"@(#)privregs.h	1.5	99/06/05 SMI" /* from SunOS psl.h 1.2 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This file is kernel isa dependent.
 */

/*
 * This file describes the cpu's privileged register set, and
 * how the machine state is saved on the stack when a trap occurs.
 */

#include <v7/sys/psr.h>
#include <sys/fsr.h>

#ifndef	_ASM

struct regs {
	long	r_psr;		/* processor status register */
	long	r_pc;		/* program counter */
	long	r_npc;		/* next program counter */
	long	r_y;		/* the y register */
	long	r_g1;		/* user global regs */
	long	r_g2;
	long	r_g3;
	long	r_g4;
	long	r_g5;
	long	r_g6;
	long	r_g7;
	long	r_o0;
	long	r_o1;
	long	r_o2;
	long	r_o3;
	long	r_o4;
	long	r_o5;
	long	r_o6;
	long	r_o7;
};

#define	r_ps	r_psr		/* for portablility */
#define	r_r0	r_o0
#define	r_sp	r_o6

#endif	/* _ASM */

#ifdef _KERNEL

#define	PIL_MAX		0xf

#define	lwptoregs(lwp)	((struct regs *)((lwp)->lwp_regs))
#define	lwptofpu(lwp)	((struct fpu *)((lwp)->lwp_fpu))

/*
 * Macros for saving/restoring registers.
 */

#define	SAVE_GLOBALS(RP) \
	st	%g1, [RP + G1*4]; \
	std	%g2, [RP + G2*4]; \
	std	%g4, [RP + G4*4]; \
	std	%g6, [RP + G6*4]; \
	mov	%y, %g1; \
	st	%g1, [RP + Y*4]

#define	RESTORE_GLOBALS(RP) \
	ld	[RP + Y*4], %g1; \
	mov	%g1, %y; \
	ld	[RP + G1*4], %g1; \
	ldd	[RP + G2*4], %g2; \
	ldd	[RP + G4*4], %g4; \
	ldd	[RP + G6*4], %g6;

#define	SAVE_OUTS(RP) \
	std	%i0, [RP + O0*4]; \
	std	%i2, [RP + O2*4]; \
	std	%i4, [RP + O4*4]; \
	std	%i6, [RP + O6*4];

#define	RESTORE_OUTS(RP) \
	ldd	[RP + O0*4], %i0; \
	ldd	[RP + O2*4], %i2; \
	ldd	[RP + O4*4], %i4; \
	ldd	[RP + O6*4], %i6;

#define	SAVE_WINDOW(SBP) \
	std	%l0, [SBP + (0*4)]; \
	std	%l2, [SBP + (2*4)]; \
	std	%l4, [SBP + (4*4)]; \
	std	%l6, [SBP + (6*4)]; \
	std	%i0, [SBP + (8*4)]; \
	std	%i2, [SBP + (10*4)]; \
	std	%i4, [SBP + (12*4)]; \
	std	%i6, [SBP + (14*4)];

#define	RESTORE_WINDOW(SBP) \
	ldd	[SBP + (0*4)], %l0; \
	ldd	[SBP + (2*4)], %l2; \
	ldd	[SBP + (4*4)], %l4; \
	ldd	[SBP + (6*4)], %l6; \
	ldd	[SBP + (8*4)], %i0; \
	ldd	[SBP + (10*4)], %i2; \
	ldd	[SBP + (12*4)], %i4; \
	ldd	[SBP + (14*4)], %i6;

#define	STORE_FPREGS(FP) \
	std	%f0, [FP]; \
	std	%f2, [FP + 8]; \
	std	%f4, [FP + 16]; \
	std	%f6, [FP + 24]; \
	std	%f8, [FP + 32]; \
	std	%f10, [FP + 40]; \
	std	%f12, [FP + 48]; \
	std	%f14, [FP + 56]; \
	std	%f16, [FP + 64]; \
	std	%f18, [FP + 72]; \
	std	%f20, [FP + 80]; \
	std	%f22, [FP + 88]; \
	std	%f24, [FP + 96]; \
	std	%f26, [FP + 104]; \
	std	%f28, [FP + 112]; \
	std	%f30, [FP + 120];

#define	LOAD_FPREGS(FP) \
	ldd	[FP], %f0; \
	ldd	[FP + 8], %f2; \
	ldd	[FP + 16], %f4; \
	ldd	[FP + 24], %f6; \
	ldd	[FP + 32], %f8; \
	ldd	[FP + 40], %f10; \
	ldd	[FP + 48], %f12; \
	ldd	[FP + 56], %f14; \
	ldd	[FP + 64], %f16; \
	ldd	[FP + 72], %f18; \
	ldd	[FP + 80], %f20; \
	ldd	[FP + 88], %f22; \
	ldd	[FP + 96], %f24; \
	ldd	[FP + 104], %f26; \
	ldd	[FP + 112], %f28; \
	ldd	[FP + 120], %f30;

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PRIVREGS_H */
