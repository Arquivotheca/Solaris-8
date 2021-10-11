/*
 * Copyright (c) 1990 - 1991, 1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)module.c	1.30	96/07/29 SMI"

#include <sys/machparam.h>
#include <sys/module.h>
#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/promif.h>

/* Generic pointer to specific routine related to modules */

/*
 * module_setup() is called from locore.s, very early. For each
 * known module type it will call the xx_module_identify() routine.
 * The xx_module_setup() routine is then called for the first module
 * that is identified. The details of module identification is left
 * to the module specific code. Typical this is just based on
 * decoding the IMPL, VERS field of the MCR. Other schemes may be
 * necessary for some module module drivers that may support multiple
 * implementations.
 *
 * module_conf.c contains the only link between module independent
 * and module dependent code. The file can be distributed in source
 * form to make porting to new modules a lot easier.
 * -- think about "module drivers".
 */


extern int	srmmu_mmu_getcr();
extern int	srmmu_mmu_getctp();
extern int	srmmu_mmu_getctx();
extern void	srmmu_mmu_setcr();
extern void	srmmu_mmu_setctp();
extern void	srmmu_mmu_setctxreg();
extern int	srmmu_mmu_probe();
extern void	srmmu_mmu_flushall();
extern void	srmmu_mmu_flushctx();
extern void	srmmu_mmu_flushrgn();
extern void	srmmu_mmu_flushseg();
extern void	srmmu_mmu_flushpage();
extern void	srmmu_mmu_flushpagectx();
extern void	srmmu_mmu_getsyncflt();
extern void	srmmu_mmu_getasyncflt();
extern int	srmmu_mmu_chk_wdreset();
extern int	srmmu_mmu_ltic();
/*
 * IFLUSH - default unimplemented
 * flush trap handler. Called from trap() and should return a 0
 * to indicate not handled
 */
extern int	srmmu_unimpflush();
extern int	sun4m_handle_ebe();
extern void	srmmu_window_overflow();
extern void	srmmu_window_underflow();
extern void	sun4m_log_module_err();

extern void	srmmu_noop();
extern int	srmmu_inoop();

int	(*v_mmu_getcr)() = srmmu_mmu_getcr;
int	(*v_mmu_getctp)() = srmmu_mmu_getctp;
int	(*v_mmu_getctx)() = srmmu_mmu_getctx;
void	(*v_mmu_setcr)() = srmmu_mmu_setcr;
void	(*v_mmu_setctp)() = srmmu_mmu_setctp;
void	(*v_mmu_setctxreg)() = srmmu_mmu_setctxreg;

int	(*v_mmu_probe)() = srmmu_mmu_probe;
void	(*v_mmu_flushall)() = srmmu_mmu_flushall;
void	(*v_mmu_flushctx)() = srmmu_mmu_flushctx;
void	(*v_mmu_flushrgn)() = srmmu_mmu_flushrgn;
void	(*v_mmu_flushseg)() = srmmu_mmu_flushseg;
void	(*v_mmu_flushpage)() = srmmu_mmu_flushpage;
void	(*v_mmu_flushpagectx)() = srmmu_mmu_flushpagectx;

int	(*v_mmu_writepte)() = srmmu_inoop;
int	(*v_mp_mmu_writepte)() = srmmu_inoop;
void	(*v_mmu_writeptp)() = srmmu_noop;

void	(*v_mmu_getsyncflt)() = srmmu_mmu_getsyncflt;
void	(*v_mmu_getasyncflt)() = srmmu_mmu_getasyncflt;
int	(*v_mmu_handle_ebe)() = sun4m_handle_ebe;
void	(*v_mmu_log_module_err)() = sun4m_log_module_err;
int	(*v_mmu_chk_wdreset)() = srmmu_mmu_chk_wdreset;

int	(*v_mmu_ltic)() = srmmu_mmu_ltic;

/*
 * IFLUSH - vector to unimplemented flush
 * handler. Default causes a BAD_TRAP
 */
int	(*v_unimpflush)() = srmmu_unimpflush;
void	(*v_ic_flush)() = srmmu_noop; /* perform individual IFLUSH */
void	(*v_mp_ic_flush)() = srmmu_noop; /* mp IFLUSH */

int	(*v_get_hwcap_flags)(int) = sparcV8_get_hwcap_flags;

void	(*v_pac_flushall)() = srmmu_noop;
int	(*v_pac_parity_chk_dis)() = srmmu_inoop;
void	(*v_pac_pageflush)() = srmmu_noop;

void	(*v_vac_usrflush)() = srmmu_noop;
void	(*v_vac_ctxflush)() = srmmu_mmu_flushctx;
void	(*v_vac_rgnflush)() = srmmu_mmu_flushrgn;
void	(*v_vac_segflush)() = srmmu_mmu_flushseg;
void	(*v_vac_flush)() = srmmu_noop;
void	(*v_vac_pageflush)() = srmmu_mmu_flushpagectx;
void	(*v_vac_allflush)() = srmmu_mmu_flushall;

void	(*v_turn_cache_on)() = srmmu_noop;
void	(*v_cache_init)() = srmmu_noop;

void    (*v_vac_color_sync)() = srmmu_noop;
void    (*v_mp_vac_color_sync)() = srmmu_noop;
void    (*v_vac_color_flush)() = srmmu_noop;


void    (*v_uncache_pt_page)() = srmmu_noop;

void    (*v_window_overflow)() = srmmu_window_overflow;
void    (*v_window_underflow)() = srmmu_window_underflow;
void    (*v_mp_window_overflow)() = srmmu_window_overflow;
void    (*v_mp_window_underflow)() = srmmu_window_underflow;

/*
 * Individual cpu modules reference externals that need not be
 * defined in other cpu modules.  These definitions are added
 * here to prevent link-time problems.
 */
int	mxcc = 0;
int	mxcc_cachesize = 0;
int	mxcc_linesize = 0;
int	mxcc_tagblockmask = 0;
int	ross_hw_workaround2 = 0;
int	ross_mod_mcr = 0;
int	ross625 = 0;
int	tsunami = 0;
int	tsunami_control_read_bug = 0;
int	tsunami_control_store_bug = 0;
int	viking = 0;
int	viking_mfar_bug = 0;
int	viking_ncload_bug = 0;
int	virtual_bcopy = 0;
int	swift_tlb_flush_bug = 0;

void
module_setup(mcr)
	int	mcr;
{
	int	i = module_info_size;
	struct module_linkage *p = module_info;

	while (i-- > 0) {
		if ((*p->identify_func)(mcr)) {
			(*p->setup_func)(mcr);
			return;
		}
		++p;
	}
	prom_printf("Unsupported module IMPL=%d VERS=%d\n\n",
		(mcr >> 28), ((mcr >> 24) & 0xf));
	prom_exit_to_mon();
	/*NOTREACHED*/
}
