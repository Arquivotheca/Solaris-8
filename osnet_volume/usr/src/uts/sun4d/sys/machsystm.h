/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_MACHSYSTM_H
#define	_SYS_MACHSYSTM_H

#pragma ident	"@(#)machsystm.h	1.18	99/06/05 SMI"

/*
 * Numerous platform-dependent interfaces that don't seem to belong
 * in any other header file.
 *
 * This file should not be included by code that purports to be
 * platform-independent.
 */

#include <sys/scb.h>
#include <sys/varargs.h>
#include <vm/hat_srmmu.h>
#include <sys/thread.h>
#include <sys/mon_clock.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL

extern void write_scb_int(int, struct trapvec *);

extern void vx_handler(char *);
extern void kvm_dup(void);

extern int Cpudelay;

extern void setintrenable(int);

extern void *kalloca(size_t, size_t, int, int);
extern void kfreea(void *, int);

extern int obpdebug;

struct cpu;
extern void init_intr_threads(struct cpu *);
extern void init_clock_thread(void);

extern struct scb *set_tbr(struct scb *);
extern void curthread_setup(struct cpu *);

extern void setsoftint(uint_t);
extern void siron(void);

extern void level15_init(void);
extern void power_off(void);

struct ptp;
struct pte;

extern uint_t n_xdbus;

extern void set_cpu_revision(void);
extern void check_options(int);
extern void level15_enable_bbus(uint_t);
extern int xdb_cpu_unit(int);
extern int get_deviceid(int nodeid, int parent);
extern void init_soft_stuffs(void);
extern uint_t disable_traps(void);
extern void enable_traps(uint_t psr_value);
extern void set_all_itr_by_cpuid(uint_t);
extern uint_t xdb_bb_status1_get(void);
extern uint_t intr_vik_action_get(void);
extern void reestablish_curthread(void);
extern int intr_prescaler_get(void);
extern void set_cpu_revision(void);
extern int intr_prof_addr(int cpuid);

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_MACHSYSTM_H */
