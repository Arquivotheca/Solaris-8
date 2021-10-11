/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_CPC_EVENT_H
#define	_SYS_CPC_EVENT_H

#pragma ident	"@(#)cpc_event.h	1.1	99/08/15 SMI"

/*
 * CPU Performance Counters measure 'events', as captured
 * by the processor dependent data structures shown below.
 * The meaning of the events, and the pattern of bits placed
 * in the control register is managed by the routines in libcpc.
 */

#include <sys/inttypes.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__sparc)

/*
 * UltraSPARC I, II and III processors
 *
 * The performance counters on these processors allow up to two 32-bit
 * performance events to be captured simultaneously from a selection
 * of metrics.   The metrics are selected by writing to the performance
 * control register, and subsequent values collected by reading from the
 * performance instrumentation counter registers.  Both registers are
 * priviliged by default, and implemented as ASRs.
 */

typedef struct _cpc_event {
	int ce_cpuver;
	hrtime_t ce_hrt;	/* gethrtime() */
	uint64_t ce_tick;	/* virtualized %tick */
	uint64_t ce_pic[2];	/* virtualized %pic */
	uint64_t ce_pcr;	/* %pcr */
} cpc_event_t;

#define	CPC_TICKREG_NAME	"%tick"
#define	CPC_TICKREG(ev)		((ev)->ce_tick)

/*
 * "Well known" bitfields in the UltraSPARC %pcr register
 * The interfaces in libcpc should make these #defines uninteresting.
 */
#define	CPC_ULTRA_PCR_USR		2
#define	CPC_ULTRA_PCR_SYS		1
#define	CPC_ULTRA_PCR_PRIVPIC		0

#define	CPC_ULTRA_PCR_PIC0_SHIFT	4
#define	CPC_ULTRA2_PCR_PIC0_MASK	UINT64_C(0xf)
#define	CPC_ULTRA3_PCR_PIC0_MASK	UINT64_C(0x3f)
#define	CPC_ULTRA_PCR_PIC1_SHIFT	11
#define	CPC_ULTRA2_PCR_PIC1_MASK	UINT64_C(0xf)
#define	CPC_ULTRA3_PCR_PIC1_MASK	UINT64_C(0x3f)

#elif defined(__i386)

/*
 * Pentium I, II and III processors
 *
 * These CPUs allow pairs of events to captured.
 * The hardware counters count up to 40-bits of significance, but
 * only allow 32 (signed) bits to be programmed into them.
 * Pentium I and Pentium II processors are programmed differently, but
 * the resulting counters and timestamps can be handled portably.
 */

typedef struct _cpc_event {
	int ce_cpuver;
	hrtime_t ce_hrt;	/* gethrtime() */
	uint64_t ce_tsc;	/* virtualized rdtsc value */
	uint64_t ce_pic[2];	/* virtualized PerfCtr[01] */
	uint32_t ce_pes[2];	/* Pentium II */
#define	ce_cesr	ce_pes[0]	/* Pentium I */
} cpc_event_t;

#define	CPC_TICKREG_NAME	"tsc"
#define	CPC_TICKREG(ev)		((ev)->ce_tsc)

/*
 * "Well known" bit fields in the Pentium CES register
 * The interfaces in libcpc should make these #defines uninteresting.
 */
#define	CPC_P5_CESR_ES0_SHIFT	0
#define	CPC_P5_CESR_ES0_MASK	0x3f
#define	CPC_P5_CESR_ES1_SHIFT	16
#define	CPC_P5_CESR_ES1_MASK	0x3f

#define	CPC_P5_CESR_OS0		6
#define	CPC_P5_CESR_USR0	7
#define	CPC_P5_CESR_CLK0	8
#define	CPC_P5_CESR_PC0		9
#define	CPC_P5_CESR_OS1		(CPC_P5_CESR_OS0 + 16)
#define	CPC_P5_CESR_USR1	(CPC_P5_CESR_USR0 + 16)
#define	CPC_P5_CESR_CLK1	(CPC_P5_CESR_CLK0 + 16)
#define	CPC_P5_CESR_PC1		(CPC_P5_CESR_PC0 + 16)

/*
 * "Well known" bit fields in the Pentium Pro PerfEvtSel registers
 * The interfaces in libcpc should make these #defines uninteresting.
 */
#define	CPC_P6_PES_INV		23
#define	CPC_P6_PES_EN		22
#define	CPC_P6_PES_INT		20
#define	CPC_P6_PES_PC		19
#define	CPC_P6_PES_E		18
#define	CPC_P6_PES_OS		17
#define	CPC_P6_PES_USR		16

#define	CPC_P6_PES_UMASK_SHIFT	8
#define	CPC_P6_PES_UMASK_MASK	(0xffu)

#define	CPC_P6_PES_CMASK_SHIFT	24
#define	CPC_P6_PES_CMASK_MASK	(0xffu)

#define	CPC_P6_PES_PIC0_MASK	(0xffu)
#define	CPC_P6_PES_PIC1_MASK	(0xffu)

#else
#error	"performance counters not available on this architecture"
#endif

/*
 * Flag arguments to cpc_bind_event and cpc_ctx_bind_event
 */
#define	CPC_BIND_LWP_INHERIT	(0x1)
#define	CPC_BIND_EMT_OVF	(0x2)

/*
 * ce_cpuver values
 */

#define	CPC_ULTRA1		1000
#define	CPC_ULTRA2		1001	/* same as ultra1 for these purposes */
#define	CPC_ULTRA3		1002

#define	CPC_PENTIUM		2000
#define	CPC_PENTIUM_MMX		2001
#define	CPC_PENTIUM_PRO		2002
#define	CPC_PENTIUM_PRO_MMX	2003

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_CPC_EVENT_H */
