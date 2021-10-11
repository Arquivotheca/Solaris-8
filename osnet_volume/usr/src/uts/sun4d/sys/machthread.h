/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_MACHTHREAD_H
#define	_SYS_MACHTHREAD_H

#pragma ident	"@(#)machthread.h	1.24	99/04/13 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	THREAD_REG	%g7		/* pointer to current thread data */

/*
 * This code *knows* device_id encodings,
 * any revision of conventions, or hardware *MUST* be reflected here!
 * XXX this should really just be reading the jtag value from some ASIC.
 */
#define	ASI_BB			0x2f
#define	BB_BASE			0xf0000000

#define	XOFF_BOOTBUS_STATUS3	0x14	/* dual only */
#define	BB_CPU_MASK		0xf8
#define	BB_CPU_SHIFT		(3 - 2)	/* 3 bits, word aligned */
#define	BB_VER2_SHIFT		3

#define	ASI_VIK_MTMP1		0x40

/*
 * Assembly macro to find address of the current CPU.
 * This way is slow but always works for sure. We use
 * this macro to setup the 'fast' lookup register, see
 * CPU_INDEX, SET_FAST_CPU_INDEX below.
 */

#define	CPU_INDEX_SLOW(scr) 						\
	.global	cpu;							\
	set	BB_BASE + (XOFF_BOOTBUS_STATUS3 << 16), scr;		\
	lduba	[scr]ASI_BB, scr;					\
	and	scr, BB_CPU_MASK, scr;					\
	srl	scr, BB_VER2_SHIFT, scr;

/*
 * Assembly macro to find address of the current CPU.
 * Used when coming in from a user trap - cannot use THREAD_REG.
 *
 * This is the fast way to get CPU ID. We use MTMP1 (Emulation Temporaries)
 * on Viking. This register is loaded with CPU ID when
 * each CPU starts. i.e. before mlsetup() for cpu0, and in cpu_startup()
 * for every other CPU, SET_FAST_CPU_INDEX is called.
 */
#define	CPU_INDEX(scr)			\
	lda	[%g0]ASI_VIK_MTMP1, scr;

#define	SET_FAST_CPU_INDEX(scr)		\
	sta	scr, [%g0]ASI_VIK_MTMP1;

#define	CPU_ADDR(reg, scr) 						\
	set	cpu, reg; /* address of cpu structure */	\
	CPU_INDEX(scr); /* get cpu index */	\
	sll	scr, 2, scr; /* convert index to word size */	\
	ld	[reg + scr], reg;

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MACHTHREAD_H */
