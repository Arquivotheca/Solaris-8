/*
 * Copyright (c) 1991, by Sun Microsystems, Inc.
 */

#ifndef _SYS_SCB_H
#define	_SYS_SCB_H

#pragma ident	"@(#)scb.h	1.3	94/05/20 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	VEC_MIN 0
#define	VEC_MAX 255
#define	AUTOBASE	16		/* base for autovectored ints */

#ifndef _ASM

typedef	struct trapvec {
	int	instr[8];
} trapvec;

/*
 * Sparc9 System control block layout
 */
struct scb {
	trapvec	tl0_hwtraps[256];	/* 0 - 255 tl0 hw traps */
	trapvec	tl0_swtraps[256];	/* 256 - 511 tl0 sw traps */
	trapvec	tl1_hwtraps[256];	/* 512 - 767 tl>0 hw traps */
	/* we don't use tl>0 sw traps */
};

#ifdef _KERNEL
extern	struct scb scb;
#endif /* _KERNEL */

#endif /* _ASM */

/*
 * These defines are used by the TL1 tlb miss handlers to calculate
 * the pc to jump to in the case the entry was not found in the TSB.
 */
#define	WTRAP_ALIGN	0x7f	/* window handlers are 128 byte align */
#define	WTRAP_FAULTOFF	124	/* last instruction in handler */

/* use the following defines to determine if trap was a fill or a spill */
#define	WTRAP_TTMASK	0x180
#define	WTRAP_TYPE	0x080


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SCB_H */
