/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_IA64_SYS_REG_H
#define	_IA64_SYS_REG_H

#pragma ident	"@(#)reg.h	1.1	99/05/04 SMI"

#include <sys/feature_tests.h>
#include <sys/stack.h>

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * Distance from beginning of thread stack (t_stk) to saved regs struct.
 */
#define	REGOFF	SA(MINFRAME)

/*
 * A gregset_t is defined as an array type for compatibility with the reference
 * source. This is important due to differences in the way the C language
 * treats arrays and structures as parameters.
 *
 * Note that _NGREG is really (sizeof (struct regs) / sizeof (greg_t)),
 */
#define	_NGREG	32	/* bogus, just for lint to work */

#ifndef _ASM

typedef uint64_t greg_t;
typedef uint64_t fpreg_t[2];
typedef greg_t	gregset_t[_NGREG];

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)

struct regs {
	greg_t	r_r0
	greg_t	r_r1;
	greg_t	r_r2;
	greg_t	r_r3;
	greg_t	r_r4;
	greg_t	r_r5;
	greg_t	r_r6;
	greg_t	r_r7;
	greg_t	r_r8;
	greg_t	r_r9;
	greg_t	r_r10;
	greg_t	r_r11;
	greg_t	r_r12;
	greg_t	r_r13;
	greg_t	r_r14;
	greg_t	r_r15;
	greg_t	r_r16;
	greg_t	r_r17;
	greg_t	r_r18;
	greg_t	r_r19;
	greg_t	r_r20;
	greg_t	r_r21;
	greg_t	r_r22;
	greg_t	r_r23;
	greg_t	r_r24;
	greg_t	r_r25;
	greg_t	r_r26;
	greg_t	r_r27;
	greg_t	r_r28;
	greg_t	r_r29;
	greg_t	r_r30;
	greg_t	r_r31;
};

#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#else /* !_ASM */

#define	R_R0	0
#define	R_R1	(R_R0 + 8)
#define	R_R2	(R_R1 + 8)
#define	R_R3	(R_R2 + 8)
#define	R_R4	(R_R3 + 8)
#define	R_R5	(R_R4 + 8)
#define	R_R6	(R_R5 + 8)
#define	R_R7	(R_R6 + 8)
#define	R_R8	(R_R7 + 8)
#define	R_R9	(R_R8 + 8)
#define	R_R10	(R_R9 + 8)
#define	R_R11	(R_R10 + 8)
#define	R_R12	(R_R11 + 8)
#define	R_R13	(R_R12 + 8)
#define	R_R14	(R_R13 + 8)
#define	R_R15	(R_R14 + 8)
#define	R_R16	(R_R15 + 8)
#define	R_R17	(R_R16 + 8)
#define	R_R18	(R_R17 + 8)
#define	R_R19	(R_R18 + 8)
#define	R_R20	(R_R19 + 8)
#define	R_R21	(R_R20 + 8)
#define	R_R22	(R_R21 + 8)
#define	R_R23	(R_R22 + 8)
#define	R_R24	(R_R23 + 8)
#define	R_R25	(R_R24 + 8)
#define	R_R26	(R_R25 + 8)
#define	R_R27	(R_R26 + 8)
#define	R_R28	(R_R27 + 8)
#define	R_R29	(R_R28 + 8)
#define	R_R30	(R_R29 + 8)
#define	R_R31	(R_R30 + 8)

#endif	/* !_ASM */

#ifdef _KERNEL
#define	lwptoregs(lwp)	((struct regs *)((lwp)->lwp_regs))
#endif /* _KERNEL */

#ifndef _ASM

typedef struct fpu {
	fpreg_t fp_reg_hi[16];
} fpregset_t;

typedef struct {
	gregset_t	gregs;		/* general register set */
	fpregset_t	fpregs;		/* floating point register set */
} mcontext_t;

#if defined(_SYSCALL32)

/* Kernel view of user ia32 registers */

#define	_NGREG32	19

typedef int32_t		greg32_t;
typedef greg32_t	gregset32_t[_NGREG32];

struct regs32 {
	greg32_t	r_gs;
	greg32_t	r_fs;
	greg32_t	r_es;
	greg32_t	r_ds;
	greg32_t	r_edi;
	greg32_t	r_esi;
	greg32_t	r_ebp;
	greg32_t	r_esp;
	greg32_t	r_ebx;
	greg32_t	r_edx;
	greg32_t	r_ecx;
	greg32_t	r_eax;
	greg32_t	r_trapno;
	greg32_t	r_err;
	greg32_t	r_eip;
	greg32_t	r_cs;
	greg32_t	r_efl;
	greg32_t	r_uesp;
	greg32_t	r_ss;
};

typedef struct fpu32 {
	union {
		struct fpchip_state		/* fp extension state */
		{
			int32_t	state[27];	/* 287/387 saved state */
			int32_t	status;		/* status word saved at */
						/* exception */
		} fpchip_state;
		struct fp_emul_space		/* for emulator(s) */
		{
			char	fp_emul[246];
			char	fp_epad[2];
		} fp_emul_space;
		int32_t	f_fpregs[62];		/* union of the above */
	} fp_reg_set;
	int32_t	f_wregs[33];		/* saved weitek state */
} fpregset32_t;

typedef struct {
	gregset32_t	gregs;		/* general register set */
	fpregset32_t	fpregs;		/* floating point register set */
} mcontext32_t;

#endif /* _SYSCALL32 */

#endif	/* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _IA64_SYS_REG_H */
