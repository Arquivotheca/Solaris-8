/*
 * Copyright (c) 1987-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpu_module.c	1.19	99/08/06 SMI"

#include <sys/types.h>
#include <sys/cpu_module.h>
#include <sys/clock.h>
#include <sys/time_impl.h>
#include <sys/lockstat.h>

/*
 * This is a dummy file that provides the default cpu module
 * that is linked to unix.
 */

uint_t	root_phys_addr_lo_mask;
int64_t timedelta;
hrtime_t hres_last_tick;
timestruc_t hrestime;
int64_t hrestime_adj;
int hres_lock;
uint_t nsec_scale;
hrtime_t hrtime_base;
int use_stick;

/*
 * This is a dummy file that provides the default cpu module
 * that is linked to unix.
 */

void
cpu_setup(void)
{}

/* ARGSUSED */
void
vtag_flushpage(caddr_t addr, uint_t ctx)
{}

/* ARGSUSED */
void
vtag_flushctx(uint_t ctx)
{}

/* ARGSUSED */
void
vtag_flushpage_tl1(uint64_t addr, uint64_t ctx)
{}

/* ARGSUSED */
void
vtag_flushctx_tl1(uint64_t ctx, uint64_t dummy)
{}

/* ARGSUSED */
void
vac_flushpage(pfn_t pf, int color)
{}

/* ARGSUSED */
void
vac_flushpage_tl1(uint64_t pf, uint64_t color)
{}

/* ARGSUSED */
void
vac_flushcolor(int color, pfn_t pf)
{}

/* ARGSUSED */
void
vac_flushcolor_tl1(uint64_t color, uint64_t dummy)
{}

/* ARGSUSED */
void
init_mondo(xcfunc_t func, uint64_t arg1, uint64_t arg2)
{}

/* ARGSUSED */
void
send_mondo(int upaid)
{}

void
fini_mondo(void)
{}

/* ARGSUSED */
void
flush_instr_mem(caddr_t addr, size_t len)
{}

void
syncfpu(void)
{}

void
ce_err(void)
{}

void
ce_err_tl1(void)
{}

void
async_err(void)
{}

void
dis_err_panic1(void)
{}

void
cpu_flush_ecache(void)
{}

void
cpu_disable_errors(void)
{}

/* It could be removed later if prom enables error handling */
void
cpu_enable_errors(void)
{}

/* ARGSUSED */
void
cpu_ce_scrub_mem_err(struct async_flt *ecc)
{}

/* ARGSUSED */
void
cpu_ce_log_err(struct async_flt *ecc)
{}

/* ARGSUSED */
int
cpu_ue_log_err(struct async_flt *ecc, char *unum)
{ return (0); }

void
clr_datapath(void)
{}

/* ARGSUSED */
void
read_ecc_data(struct async_flt *ecc, short verbose, short ce_err)
{}

/* ARGSUSED */
void
itlb_rd_entry(uint_t entry, tte_t *tte, caddr_t *addr, int *ctxnum)
{}

/* ARGSUSED */
void
dtlb_rd_entry(uint_t entry, tte_t *tte, caddr_t *addr, int *ctxnum)
{}

u_longlong_t
gettick(void)
{ return (0); }

/* ARGSUSED */
void
gethrestime(timespec_t *tp)
{}

/* ARGSUSED */
int
lockstat_event_start(uintptr_t lp, ls_pend_t *lpp)
{ return (0); }

/* ARGSUSED */
hrtime_t
lockstat_event_end(ls_pend_t *lpp)
{ return (0); }

hrtime_t
gethrtime(void)
{ return (0); }

hrtime_t
gethrtime_unscaled(void)
{ return (0); }

hrtime_t
gethrtime_initial(void)
{ return (0); }

uint_t
get_impl(void)
{ return (0); }

hrtime_t
get_hrestime(void)
{ return (0); }

ulong_t
get_timestamp(void)
{ return (0); }

ulong_t
get_virtime(void)
{ return (0); }

hrtime_t
gethrtime_max(void)
{ return (0); }

/*ARGSUSED*/
void
scalehrtime(hrtime_t *hrt)
{}

void
hres_tick(void)
{}
