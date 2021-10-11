/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_MACHSYSTM_H
#define	_SYS_MACHSYSTM_H

#pragma ident	"@(#)machsystm.h	1.27	99/06/05 SMI"

/*
 * Numerous platform-dependent interfaces that don't seem to belong
 * in any other header file.
 *
 * This file should not be included by code that purports to be
 * platform-independent.
 */

#include <sys/scb.h>
#include <sys/varargs.h>
#include <sys/vmem.h>
#include <vm/hat_srmmu.h>
#include <sys/mon_clock.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL

extern void write_scb_int(int, struct trapvec *);

extern void mp_halt(char *);

extern void enable_dvma(void);
extern void disable_dvma(void);
extern void set_intreg(int, int);

extern void vx_handler(char *);
extern void kvm_dup(void);

extern int Cpudelay;
extern void setcpudelay(void);

extern void setintrenable(int);

extern unsigned int vac_mask;
extern int vac_hashwusrflush;

extern void pac_flushall(void);

extern int dvmasize;
extern vmem_t *dvmamap;
extern char DVMA[];		/* on 4m ?? */

extern int pte2atype(void *, ulong_t, ulong_t *, uint_t *);

extern void ppmapinit(void);

extern void *kalloca(size_t, size_t, int, int);
extern void kfreea(void *, int);
extern void *knc_alloc(vmem_t *, size_t, int);
extern vmem_t *knc_arena;
extern size_t knc_limit;

extern int sx_vrfy_pfn(uint_t, uint_t);

extern int obpdebug;

struct cpu;
extern void init_intr_threads(struct cpu *);
extern void init_clock_thread(void);

extern struct scb *set_tbr(struct scb *);
extern void reestablish_curthread(void);

extern void set_intmask(int, int);
extern void set_itr_bycpu(int);
extern void send_dirint(int, int);
extern void setsoftint(uint_t);
extern void siron(void);

extern void set_interrupt_target(int);
extern int getprocessorid(void);
extern int swapl(int, int *);
extern int atomic_tas(int *);

extern void memctl_getregs(uint_t *, uint_t *, uint_t *);
extern void memctl_set_enable(uint_t, uint_t);
extern void msi_sync_mode(void);

extern ulong_t get_sfsr(void);
extern void flush_writebuffers(void);
extern void flush_writebuffers_to(caddr_t);

struct regs;
extern void vik_fixfault(struct regs *, caddr_t *, uint_t);

struct memlist;
extern u_longlong_t get_max_phys_size(struct memlist *);

extern void bpt_reg(uint_t, uint_t);
extern void turn_cache_on(int);
extern void cache_init(void);

struct ptp;
struct pte;
extern void mmu_readpte(struct pte *, struct pte *);
/* mmu_readptp is actually mmu_readpte */
extern void mmu_readptp(struct ptp *, struct ptp *);

extern void rd_ptbl_as(struct as **, struct as **);
extern void rd_ptbl_base(caddr_t *, caddr_t *);

extern void rd_ptbl_next(struct ptbl **, struct ptbl **);
extern void rd_ptbl_prev(struct ptbl **, struct ptbl **);

extern void rd_ptbl_parent(struct ptbl **, struct ptbl **);

extern void rd_ptbl_flags(uchar_t *, uchar_t *);
extern void rd_ptbl_vcnt(uchar_t *, uchar_t *);
extern void rd_ptbl_lcnt(ushort_t *, ushort_t *);

extern const char sbus_to_sparc_tbl[];
extern void sbus_set_64bit(uint_t slot);

extern int vac;
extern int cache;
extern int use_cache;
extern int use_ic;
extern int use_dc;
extern int use_ec;
extern int use_mp;
extern int use_vik_prefetch;
extern int use_mxcc_prefetch;
extern int use_store_buffer;
extern int use_multiple_cmds;
extern int use_rdref_only;
extern int use_table_walk;
extern int use_mix;
extern int do_pg_coloring;
extern int use_page_coloring;
extern int mxcc;
extern int pokefault;
extern uint_t module_wb_flush;
extern volatile uint_t aflt_ignored;
extern volatile uint_t system_fatal;
extern int ross_iobp_workaround;
extern int ross_hw_workaround2;
extern int ross_hd_bug;
extern char tbr_wr_addr_inited;
extern int nvsimm_present;
extern int ross_iopb_workaround;

extern uint_t cpu_nodeid[];

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_MACHSYSTM_H */
