/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_MACHTHREAD_H
#define	_SYS_MACHTHREAD_H

#pragma ident	"@(#)machthread.h	1.13	99/04/13 SMI"

#include <sys/asi.h>
#include <sys/spitasi.h>
#include <sys/bitmap.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	THREAD_REG	%g7		/* pointer to current thread data */

#ifdef	_STARFIRE
/*
 * CPU_INDEX(r, scr)
 * Returns cpu id in r.
 * On Starfire, this is read from the Port Controller's Port ID
 * register in local space.
 *
 * Need to load the 64 bit address of the PC's PortID reg
 * using only one register. Kludge the 41 bits address constant to
 * be 32bits by shifting it 12 bits to the right first.
 */
#define	LOCAL_PC_PORTID_ADDR_SRL12 0x1FFF4000
#define	PC_PORT_ID 0xD0

#define	CPU_INDEX(r, scr)			\
	rdpr	%pstate, scr;			\
	andn	scr, PSTATE_IE | PSTATE_AM, r;	\
	wrpr	r, 0, %pstate;			\
	set	LOCAL_PC_PORTID_ADDR_SRL12, r;  \
	sllx    r, 12, r;                       \
	or	r, PC_PORT_ID, r;		\
	lduwa	[r]ASI_IO, r;			\
	wrpr	scr, 0, %pstate
#else
/*
 * CPU_INDEX(r, scr)
 * Returns cpu id in r.
 * On Sun5 machines, this is equivalent to the mid field of the
 * UPA Config register.
 * XXX - scr reg is not used here.
 */
#define	CPU_INDEX(r, scr)			\
	ldxa	[%g0]ASI_UPA_CONFIG, r;	\
	srlx	r, 17, r;		\
	and	r, 0x1F, r
#endif	/* _STARFIRE */

/*
 * Given a cpu id extract the appropriate word
 * in the cpuset mask for this cpu id.
 */
#if CPUSET_SIZE > CLONGSIZE
#define	CPU_INDEXTOSET(base, index, scr)	\
	srl	index, BT_ULSHIFT, scr;		\
	and	index, BT_ULMASK, index;	\
	sll	scr, CLONGSHIFT, scr;		\
	add	base, scr, base
#else
#define	CPU_INDEXTOSET(base, index, scr)
#endif	/* CPUSET_SIZE */


/*
 * Assembly macro to find address of the current CPU.
 * Used when coming in from a user trap - cannot use THREAD_REG.
 * Args are destination register and one scratch register.
 */
#define	CPU_ADDR(reg, scr) 		\
	.global	cpu;			\
	CPU_INDEX(scr, reg);		\
	sll	scr, CPTRSHIFT, scr;	\
	set	cpu, reg;		\
	ldn	[reg + scr], reg

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MACHTHREAD_H */
