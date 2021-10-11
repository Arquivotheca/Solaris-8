/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_IA32_SYS_REG_H
#define	_IA32_SYS_REG_H

#pragma ident	"@(#)reg.h	1.3	99/08/05 SMI"

#ifndef _ASM
#include <sys/types.h>
#endif

#include <sys/feature_tests.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Location of the users' stored registers relative to EAX.
 * Usage is u.u_ar0[XX].
 * Note: The names and offsets defined here are specified by ABI
 *	 Intel386 processor supplement.
 */

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	SS		18	/* only stored on a privilege transition */
#define	UESP		17	/* only stored on a privilege transition */
#define	EFL		16
#define	CS		15
#define	EIP		14
#define	ERR		13
#define	TRAPNO		12
#define	EAX		11
#define	ECX		10
#define	EDX		9
#define	EBX		8
#define	ESP		7
#define	EBP		6
#define	ESI		5
#define	EDI		4
#define	DS		3
#define	ES		2
#define	FS		1
#define	GS		0

/* aliases for portability */

#define	PC	EIP
#define	USP	UESP
#define	SP	ESP
#define	PS	EFL
#define	R0	EAX
#define	R1	EDX

/*
 * Distance from beginning of thread stack (t_stk) to saved regs struct.
 */
#define	REGOFF	MINFRAME
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

/*
 * A gregset_t is defined as an array type for compatibility with the reference
 * source. This is important due to differences in the way the C language
 * treats arrays and structures as parameters.
 *
 * Note that NGREG is really (sizeof (struct regs) / sizeof (greg_t)),
 * but that the ABI defines it absolutely to be 19.
 */
#define	_NGREG	19

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	NGREG	_NGREG
#endif

#ifndef _ASM

typedef int32_t	greg_t;
typedef greg_t	gregset_t[_NGREG];

/*
**  This is a template used by trap() and syscall() to find saved copies
**  of the registers on the stack.
*/

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
struct regs {
	greg_t	r_gs;
	greg_t	r_fs;
	greg_t	r_es;
	greg_t	r_ds;
	greg_t	r_edi;
	greg_t	r_esi;
	greg_t	r_ebp;
	greg_t	r_esp;
	greg_t	r_ebx;
	greg_t	r_edx;
	greg_t	r_ecx;
	greg_t	r_eax;
	greg_t	r_trapno;
	greg_t	r_err;
	greg_t	r_eip;
	greg_t	r_cs;
	greg_t	r_efl;
	greg_t	r_uesp;
	greg_t	r_ss;
};

#define	r_r0	r_eax		/* r0 for portability */
#define	r_r1	r_edx		/* r1 for portability */
#define	r_usp	r_uesp		/* user's stack pointer */
#define	r_sp	r_esp		/* system stack pointer */
#define	r_pc	r_eip		/* user's instruction pointer */
#define	r_ps	r_efl		/* user's EFLAGS */
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

#if defined(_KERNEL) || defined(_BOOT)
/*
 *  This structure is used by bootops to save/restore registers when
 *  transferring between protected and realmode code.
 *
 *  NOTE: The following macros require an ANSI compiler!
 */

#define	i8080reg(r) union {  /* 8080-like "general purpose" registers */\
	uint32_t e ## r ## x;					\
	struct { uint16_t r ## x; } word;				\
	struct { uint8_t  r ## l, r ## h; } byte;			\
}

#define	i8086reg(r) union { /* 16/32-bit "special purpose" registers  */\
	uint32_t e ## r;						\
	struct { uint16_t r; } word;				\
}

struct bop_regs {
	/*
	 *  Machine state structure for realmode <-> protected mode callout
	 *  operations:
	 */

	i8080reg(a) eax;	/* The so-called "general purpose" registers */
	i8080reg(d) edx;
	i8080reg(c) ecx;
	i8080reg(b) ebx;

	i8086reg(bp) ebp;	/* 16/32-bit "pointer" registers */
	i8086reg(si) esi;
	i8086reg(di) edi;

	uint16_t ds;	/* Segment registers */
	uint16_t es;
	uint16_t fs;
	uint16_t gs;

	uint32_t eflags;
};

#undef	i8080reg
#undef	i8086reg
#endif	/* _KERNEL || _BOOT */
#endif	/* !_ASM */

#ifdef _KERNEL
#define	lwptoregs(lwp)	((struct regs *)((lwp)->lwp_regs))
#endif /* _KERNEL */

/* CR0 Register */


#if !defined(_XPG4_2) || defined(__EXTENSIONS__)

#define	CR0_PG	0x80000000		/* paging enabled 		*/
#define	CR0_CE	0x40000000		/* cache enable (486)		*/
#define	CR0_WT	0x20000000		/* writes transparent (486)	*/
#define	CR0_AM	0x00040000		/* alignment mask (486)		*/
#define	CR0_WP	0x00010000		/* write prot. sup access (486)	*/
#define	CR0_NE	0x00000020		/* fp exception interrupt (486)	*/
#define	CR0_ET	0x00000010		/* extension type 		*/
#define	CR0_TS	0x00000008		/* task switch			*/
#define	CR0_EM	0x00000004		/* emulation mode		*/
#define	CR0_MP	0x00000002		/* math present			*/
#define	CR0_PE	0x00000001		/* protection enabled		*/

#define	CR0_CD	CR0_CE			/* name of bit in newer docs	*/
#define	CR0_NW	CR0_WT			/* name of bit in newer docs	*/

/* CR3 Register (486) */

#define	CR3_PCD	0x00000010		/* cache disable 		*/
#define	CR3_PWT 0x00000008		/* write through 		*/

/* CR4 Register (Pentium and beyond) */

#define	CR4_VME	0x00000001		/* virtual-8086 mode extensions	*/
#define	CR4_PVI	0x00000002		/* protected-mode virtual interrupts */
#define	CR4_TSD	0x00000004		/* time stamp disable		*/
#define	CR4_DE	0x00000008		/* debugging extensions		*/
#define	CR4_PSE	0x00000010		/* page size extensions		*/
#define	CR4_PAE	0x00000020		/* physical address extension	*/
#define	CR4_MCE	0x00000040		/* machine check enable		*/
#define	CR4_PGE	0x00000080		/* page global enable		*/
#define	CR4_PCE	0x00000100		/* perf-monitoring counter enable */

/* Control register layout for panic dump */

#define	CREGSZ		36
#define	CREG_GDT	0
#define	CREG_IDT	8
#define	CREG_LDT	16
#define	CREG_TASKR	18
#define	CREG_CR0	20
#define	CREG_CR2	24
#define	CREG_CR3	28
#define	CREG_CR4	32

#ifndef _ASM

struct cregs {
	uint64_t	cr_gdt;
	uint64_t	cr_idt;
	uint16_t	cr_ldt;
	uint16_t	cr_task;
	uint32_t	cr_cr0;
	uint32_t	cr_cr2;
	uint32_t	cr_cr3;
	uint32_t	cr_cr4;
};

extern void	getcregs(struct cregs *);

/*
 * Floating point definitions.
 * Note: This structure definition is specified in the ABI
 *	 Intel386 processor supplement.
 */

typedef struct fpu {
	union {
		struct fpchip_state		/* fp extension state */
		{
			int 	state[27];	/* 287/387 saved state */
			int 	status;		/* status word saved at */
						/* exception */
		} fpchip_state;
		struct fp_emul_space		/* for emulator(s) */
		{
			char	fp_emul[246];
			char	fp_epad[2];
		} fp_emul_space;
		int 	f_fpregs[62];		/* union of the above */
	} fp_reg_set;
	long    	f_wregs[33];		/* saved weitek state */
} fpregset_t;

#define	NDEBUGREG	8

typedef struct dbregset {
	unsigned	debugreg[NDEBUGREG];
} dbregset_t;

/* Note: This structue is specified in the ABI Intel386 processor supplement */
typedef struct {
	gregset_t	gregs;		/* general register set */
	fpregset_t	fpregs;		/* floating point register set */
} mcontext_t;

#endif	/* _ASM */
#endif /* !defined(_XPG4_2) || defined(__EXTENSIONS__) */

/*
 * The following is here for XPG4.2 standards compliance.
 * reg.h is included in ucontext.h for the definition of
 * mcontext_t, which breaks XPG4.2 namespace.
 */
#if defined(_XPG4_2) && !defined(__EXTENSIONS__)
#ifndef _ASM
/*
 * Floating point definitions.
 * Note: This structure definition is specified in the ABI
 *	 Intel386 processor supplement.
 */
typedef struct __fpu {
	union {
		struct __fpchip_state		/* fp extension state */
		{
			int 	__state[27];	/* 287/387 saved state */
			int 	__status;	/* status word saved at */
						/* exception */
		} __fpchip_state;
		struct __fp_emul_space		/* for emulator(s) */
		{
			char	__fp_emul[246];
			char	__fp_epad[2];
		} __fp_emul_space;
		int 	__f_fpregs[62];		/* union of the above */
	} __fp_reg_set;
	long    	__f_wregs[33];		/* saved weitek state */
} fpregset_t;

/* Note: This structue is specified in the ABI Intel386 processor supplement */
typedef struct {
	gregset_t	__gregs;	/* general register set */
	fpregset_t	__fpregs;	/* floating point register set */
} mcontext_t;
#endif	/* _ASM */
#endif /* defined(_XPG4_2) && !defined(__EXTENSIONS__) */

#ifdef	__cplusplus
}
#endif

#endif	/* _IA32_SYS_REG_H */
