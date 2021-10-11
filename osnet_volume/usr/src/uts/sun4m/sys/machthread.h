/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_MACHTHREAD_H
#define	_SYS_MACHTHREAD_H

#pragma ident	"@(#)machthread.h	1.18	99/04/13 SMI"

#include <sys/mmu.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	THREAD_REG	%g7		/* pointer to current thread data */

/*
 * Assembly macro to find address of the current CPU.
 * Used when coming in from a user trap - cannot use THREAD_REG.
 * Args are destination register and one scratch register.
 */
#define	CPU_ADDR(reg, scr) 		\
	.global	cpu;			\
	CPU_INDEX(scr);			\
	sll	scr, 2, scr;		\
	set	cpu, reg;		\
	ld	[reg + scr], reg

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MACHTHREAD_H */
