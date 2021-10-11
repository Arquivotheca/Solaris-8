/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_MACHSYSTM_H
#define	_SYS_MACHSYSTM_H

#pragma ident	"@(#)machsystm.h	1.59	99/10/01 SMI"

/*
 * Numerous platform-dependent interfaces that don't seem to belong
 * in any other header file.
 *
 * This file should not be included by code that purports to be
 * platform-independent.
 */

#include <sys/types.h>
#include <sys/scb.h>
#include <sys/varargs.h>
#include <sys/machparam.h>
#include <sys/thread.h>
#include <vm/seg_enum.h>
#include <sys/sunddi.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL
/*
 * The following enum types determine how interrupts are distributed
 * on a sun4u system.
 */
enum intr_policies {
	/*
	 * Target interrupt at the CPU running the add_intrspec
	 * thread. Also used to target all interrupts at the panicing
	 * CPU.
	 */
	INTR_CURRENT_CPU = 0,

	/*
	 * Target all interrupts at the boot cpu
	 */
	INTR_BOOT_CPU,

	/*
	 * Flat distribution of all interrupts
	 */
	INTR_FLAT_DIST
};


/*
 * Structure that defines the interrupt distribution list. It contains
 * enough info about the interrupt so that it can callback the parent
 * nexus driver and retarget the interrupt to a different CPU.
 */
struct intr_dist {
	struct intr_dist *next;	/* link to next in list */
	void (*func)(void *, int, uint_t);	/* Callback function */
	void *dip;		/* Nexus parent callback arg 1 */
	int mondo;		/* Nexus parennt callback arg 2 */
	int mask_flag;		/* Mask off lower 3 bits when searching? */
};

/*
 * Miscellaneous cpu_state changes
 */
extern void power_down(const char *);
extern void do_shutdown(void);

/*
 * Number of seconds until power is shut off
 */
extern int thermal_powerdown_delay;


/*
 * prom-related
 */
extern int obpdebug;
extern uint_t tba_taken_over;
extern int vx_entered;
extern kmutex_t prom_mutex;
extern kcondvar_t prom_cv;
extern void forthdebug_init(void);
extern void init_vx_handler(void);
extern void kern_preprom(void);
extern void kern_postprom(void);


/*
 * Trap-related
 */
struct regs;
extern void trap(struct regs *rp, caddr_t addr, uint32_t type,
    uint32_t mmu_fsr);
extern void *set_tba(void *);
extern caddr_t set_trap_table(void);

/*
 * misc. primitives
 */
extern void debug_flush_windows(void);
extern void flush_windows(void);
extern int getprocessorid(void);
extern void reestablish_curthread(void);

extern void stphys(uint64_t physaddr, int value);
extern int ldphys(uint64_t physaddr);
extern void stdphys(uint64_t physaddr, uint64_t value);
extern uint64_t lddphys(uint64_t physaddr);

extern void stphysio(u_longlong_t physaddr, uint_t value);
extern uint_t ldphysio(u_longlong_t physaddr);
extern void sthphysio(u_longlong_t physaddr, ushort_t value);
extern ushort_t ldhphysio(u_longlong_t physaddr);
extern void stbphysio(u_longlong_t physaddr, uchar_t value);
extern uchar_t ldbphysio(u_longlong_t physaddr);
extern void stdphysio(u_longlong_t physaddr, u_longlong_t value);
extern u_longlong_t lddphysio(u_longlong_t physaddr);

extern uint32_t swapl(uint32_t *, uint32_t);

extern int pf_is_dmacapable(pfn_t);

/*
 * bootup-time
 */
extern int ncpunode;
extern int niobus;

extern void segnf_init(void);
extern void kern_setup1(void);
extern void startup(void);
extern void post_startup(void);
extern void install_va_to_tte(void);
extern void setwstate(uint_t);
extern void create_va_to_tte(void);
extern int memscrub_init(void);

/*
 * Interrupts
 */
struct cpu;
extern struct cpu cpu0;
extern size_t intr_add_pools;
extern struct intr_req *intr_add_head;
extern struct intr_req *intr_add_tail;
extern struct scb *set_tbr(struct scb *);

extern void init_intr_threads(struct cpu *);
extern uint_t disable_vec_intr(void);
extern void enable_vec_intr(uint_t);
extern void setintrenable(int);
extern uint_t intr_add_cpu(void (*func)(void *, int, uint_t),
	void *, int, int);
extern void intr_rem_cpu(int);
extern void intr_redist_all_cpus(enum intr_policies);
extern struct intr_dist *intr_exist(void *dip);
extern void intr_update_cb_data(struct intr_dist *iptr, void *dip);
extern void send_dirint(int, int);
extern void setsoftint(uint_t);
extern void siron(void);

/*
 * Time- and %tick-related
 */
extern void tick_write_delta(uint64_t);
extern void tickcmpr_set(uint64_t);
extern void tickcmpr_reset(void);
extern void tickcmpr_disable(void);
extern int tickcmpr_disabled(void);
extern void tickcmpr_enqueue_req(void);
extern void tickcmpr_dequeue_req(void);

extern void clear_soft_intr(uint_t);

/*
 * Caches
 */
extern int vac;
extern int cache;
extern int use_cache;
extern int use_ic;
extern int use_dc;
extern int use_ec;
extern int use_mp;
extern uint_t vac_mask;
extern uint64_t ecache_flushaddr;
extern int ecache_linesize;
extern int ecache_size;

/*
 * VM
 */
extern int do_pg_coloring;
extern int do_virtual_coloring;
extern int use_page_coloring;
extern int use_virtual_coloring;

extern caddr_t ndata_alloc_cpus(caddr_t);
extern caddr_t ndata_alloc_dmv(caddr_t);
extern caddr_t ndata_alloc_hat(caddr_t, caddr_t, pgcnt_t, long *);
extern caddr_t ndata_alloc_page_freelists(caddr_t);
extern caddr_t alloc_more_hblks(caddr_t, long);
extern size_t page_ctrs_sz(void);
extern caddr_t page_ctrs_alloc(caddr_t);
extern void page_freelist_coalesce(int);
extern void ppmapinit(void);

/*
 * VIS-accelerated copy/zero
 */
extern int use_hw_bcopy;
extern int use_hw_copyio;
extern int use_hw_bzero;

/*
 * MP
 */
extern void idle_other_cpus(void);
extern void resume_other_cpus(void);
extern void stop_other_cpus(void);
extern void idle_stop_xcall(void);
extern void set_idle_cpu(int);
extern void unset_idle_cpu(int);

/*
 * Error handling
 */
extern int ce_verbose;

extern void set_error_enable(uint64_t neer);
extern uint64_t get_error_enable(void);
extern void get_asyncflt(uint64_t *afsr);
extern void set_asyncflt(uint64_t afsr);
extern void get_asyncaddr(uint64_t *afar);
extern void scrubphys(uint64_t paddr, int ecache_size);
extern void clearphys(uint64_t paddr, int ecache_size);
extern void set_error_enable_tl1(uint64_t neer, uint64_t dummy);

/*
 * Prototypes which really belongs to sunddi.c, and should be moved to
 * sunddi.c if there is another platform using these calls.
 */
dev_info_t *
e_ddi_nodeid_to_dip(dev_info_t *dip, uint_t nodeid);

#ifdef __sparcv9
/*
 * Constants which define the "hole" in the 64-bit sfmmu address space.
 * These are set to specific values by the CPU module code.
 */
extern caddr_t	hole_start, hole_end;

#define	INVALID_VADDR(a)	(((a) >= hole_start && (a) < hole_end))

#else

#ifdef	lint

#define	INVALID_VADDR(a)	(__lintzero)

#else

#define	INVALID_VADDR(a)	0

#endif	/* lint */
#endif	/* __sparcv9 */


#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_MACHSYSTM_H */
