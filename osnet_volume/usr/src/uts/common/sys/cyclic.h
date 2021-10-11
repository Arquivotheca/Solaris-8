/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_CYCLIC_H
#define	_SYS_CYCLIC_H

#pragma ident	"@(#)cyclic.h	1.1	99/06/05 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _ASM
#include <sys/time.h>
#include <sys/cpuvar.h>
#include <sys/cpupart.h>
#endif /* !_ASM */

#define	CY_LOW_LEVEL		0
#define	CY_LOCK_LEVEL		1
#define	CY_HIGH_LEVEL		2
#define	CY_SOFT_LEVELS		2
#define	CY_LEVELS		3

#ifndef _ASM

typedef uintptr_t cyclic_id_t;
typedef int cyc_index_t;
typedef int cyc_cookie_t;
typedef uint16_t cyc_level_t;
typedef void (*cyc_func_t)(void *);
typedef void *cyb_arg_t;

#define	CYCLIC_NONE		((cyclic_id_t)0)

typedef struct cyc_handler {
	cyc_func_t cyh_func;
	void *cyh_arg;
	cyc_level_t cyh_level;
} cyc_handler_t;

typedef struct cyc_time {
	hrtime_t cyt_when;
	hrtime_t cyt_interval;
} cyc_time_t;

#ifdef _KERNEL

extern cyclic_id_t cyclic_add(cyc_handler_t *, cyc_time_t *);
extern void cyclic_remove(cyclic_id_t);
extern void cyclic_bind(cyclic_id_t, cpu_t *, cpupart_t *);
extern hrtime_t cyclic_getres();

extern int cyclic_offline(cpu_t *cpu);
extern void cyclic_online(cpu_t *cpu);
extern int cyclic_juggle(cpu_t *cpu);
extern void cyclic_move_in(cpu_t *);
extern int cyclic_move_out(cpu_t *);
extern void cyclic_suspend();
extern void cyclic_resume();

extern void cyclic_fire(cpu_t *cpu);
extern void cyclic_softint(cpu_t *cpu, cyc_level_t level);

#endif /* _KERNEL */

#endif /* !_ASM */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_CYCLIC_H */
