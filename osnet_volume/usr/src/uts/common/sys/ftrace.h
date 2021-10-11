/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_FTRACE_H
#define	_SYS_FTRACE_H

#pragma ident	"@(#)ftrace.h	1.2	99/07/12 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Constants used by both asm and non-asm code.
 */

/*
 * Flags determining the state of tracing -
 *   both for the "ftrace_state" variable, and for the per-CPU variable
 *   "cpu[N]->cpu_ftrace_state".
 */
#define	FTRACE_READY	0x00000001
#define	FTRACE_ENABLED	0x00000002

#if !defined(_ASM)

#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <sys/types.h>

/*
 * The record of a single event.
 *
 * Should fit nicely into a standard cache line.
 * Here, the 32-bit version is 32 bytes, and the 64-bit version is 64 bytes.
 */
typedef struct ftrace_record {
	char		*ftr_event;
	kthread_t	*ftr_thread;
	uint64_t	ftr_tick;
	caddr_t		ftr_caller;
	ulong_t		ftr_data1;
	ulong_t		ftr_data2;
	ulong_t		ftr_data3;
#ifdef	_LP64
	ulong_t		__pad;
#endif
} ftrace_record_t;

/*
 * Default per-CPU event ring buffer size.
 */
#define	FTRACE_NENT 1024

#ifdef _KERNEL

/*
 * Tunable parameters in /etc/system.
 */
extern int ftrace_atboot;	/* Whether to start fast tracing on boot. */
extern int ftrace_nent;		/* Size of the per-CPU event ring buffer. */

extern int		ftrace_cpu_setup(cpu_setup_t, int, void *);
extern void		ftrace_init(void);
extern int		ftrace_start(void);
extern int		ftrace_stop(void);
extern void		ftrace_0(char *);
extern void		ftrace_1(char *, ulong_t);
extern void		ftrace_2(char *, ulong_t, ulong_t);
extern void		ftrace_3(char *, ulong_t, ulong_t, ulong_t);

#define	FTRACE_0(fmt)						\
	{							\
		if (CPU->cpu_ftrace.ftd_state & FTRACE_ENABLED)	\
			ftrace_0(fmt);				\
	}
#define	FTRACE_1(fmt, d1) 					\
	{							\
		if (CPU->cpu_ftrace.ftd_state & FTRACE_ENABLED)	\
			ftrace_1(fmt, d1);			\
	}
#define	FTRACE_2(fmt, d1, d2) 					\
	{							\
		if (CPU->cpu_ftrace.ftd_state & FTRACE_ENABLED)	\
			ftrace_2(fmt, d1, d2);			\
	}
#define	FTRACE_3(fmt, d1, d2, d3) 				\
	{							\
		if (CPU->cpu_ftrace.ftd_state & FTRACE_ENABLED)	\
			ftrace_3(fmt, d1, d2, d3);		\
	}
#define	FTRACE_START()	ftrace_start()
#define	FTRACE_STOP()	ftrace_stop()

#endif	/* _KERNEL */

#endif	/* !defined(_ASM) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FTRACE_H */
