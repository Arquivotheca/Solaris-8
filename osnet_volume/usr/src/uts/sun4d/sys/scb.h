/*
 * Copyright (c) 1989 by Sun Microsystems, Inc.
 */

#ifndef	_SYS_SCB_H
#define	_SYS_SCB_H

#pragma ident	"@(#)scb.h	1.7	94/12/28 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	VEC_MIN 64
#define	VEC_MAX 255
#define	AUTOBASE	16		/* base for autovectored ints */

#ifndef	_ASM

typedef	struct trapvec {
	int	instr[4];
} trapvec;

/*
 * Sparc System control block layout
 */

struct scb {
	trapvec reset;			/* 0 reset */
	trapvec inst_access;		/* 1 instruction access */
	trapvec illegal_inst;		/* 2 illegal instruction */
	trapvec priv_inst;		/* 3 privilegded instruction */
	trapvec fp_disabled;		/* 4 floating point desabled */
	trapvec window_overflow;	/* 5 window overflow */
	trapvec window_underflow;	/* 6 window underflow */
	trapvec alignment_excp;		/* 7 alignment */
	trapvec fp_exception;		/* 8 floating point exception */
	trapvec	data_access;		/* 9 protection violation */
	trapvec	tag_overflow;		/* 10 tag overflow taddtv */
	trapvec reserved[5];		/* 11 - 15 */
	trapvec stray;			/* 16 spurious */
	trapvec	interrupts[15];		/* 17 - 31 autovectors */
	trapvec impossible[96];		/* 32 - 127 "cannot happen" */
	trapvec user_trap[128];		/* 128 - 255 */
};

#ifdef	_KERNEL
extern	struct scb scb;
#endif	/* _KERNEL */

#endif	/* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SCB_H */
