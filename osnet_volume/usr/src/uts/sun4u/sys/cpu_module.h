/*
 * Copyright (c) 1987-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_CPU_MODULE_H
#define	_SYS_CPU_MODULE_H

#pragma ident	"@(#)cpu_module.h	1.18	99/06/14 SMI"

#include <sys/pte.h>
#include <sys/async.h>
#include <sys/x_call.h>

#ifdef	__cplusplus
extern "C" {
#endif


#ifdef _KERNEL

/*
 * The are functions that are expected of the cpu modules.
 */

extern struct module_ops *moduleops;

/*
 * module initialization
 */
void	cpu_setup(void);

/*
 * virtual demap flushes (tlbs & virtual tag caches)
 */
void	vtag_flushpage(caddr_t addr, uint_t ctx);
void	vtag_flushctx(uint_t ctx);
void	vtag_flushpage_tl1(uint64_t addr, uint64_t ctx);
void	vtag_flushctx_tl1(uint64_t ctx, uint64_t dummy);

/*
 * virtual alias flushes (virtual address caches)
 */
void	vac_flushpage(pfn_t pf, int color);
void	vac_flushpage_tl1(uint64_t pf, uint64_t color);
void	vac_flushcolor(int color, pfn_t pf);
void	vac_flushcolor_tl1(uint64_t color, uint64_t dummy);

/*
 * sending x-calls
 */
void	init_mondo(xcfunc_t *func, uint64_t arg1, uint64_t arg2);
void	send_mondo(int upaid);
void	fini_mondo(void);

/*
 * flush instruction cache if needed
 */
void	flush_instr_mem(caddr_t addr, size_t len);

/*
 * take pending fp traps if fpq present
 * this function is also defined in fpusystm.h
 */
void	syncfpu(void);

/*
 * Cpu-specific error and ecache handling routines
 */
void	ce_err(void);
void	ce_err_tl1(void);
void	async_err(void);
void	dis_err_panic1(void);
void	cpu_flush_ecache(void);
void	cpu_disable_errors(void);
/* It could be removed later if prom enables errors */
void	cpu_enable_errors(void);
void	cpu_ce_scrub_mem_err(struct async_flt *ecc);
void	cpu_ce_log_err(struct async_flt *ecc);
int	cpu_ue_log_err(struct async_flt *ecc, char *unum);
void	read_ecc_data(struct async_flt *ecc, short verbose, short ce_err);
/* add clr_datapath to aviod lint warning for ac_test.c temporarily */
void	clr_datapath(void);

/*
 * retrieve information from the specified tlb entry. these functions are
 * called by "cpr" module
 */
void	itlb_rd_entry(uint_t entry, tte_t *tte, caddr_t *addr, int *ctxnum);
void	dtlb_rd_entry(uint_t entry, tte_t *tte, caddr_t *addr, int *ctxnum);

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_CPU_MODULE_H */
