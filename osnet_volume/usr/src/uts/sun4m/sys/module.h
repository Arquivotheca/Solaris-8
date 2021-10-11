/*
 * Copyright (c) 1987-1993 by Sun Microsystems, Inc.
 */

#ifndef _SYS_MODULE_H
#define	_SYS_MODULE_H

#pragma ident	"@(#)module.h	1.26	95/01/16 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * for all module type IDs,
 * the low 8 bits is the same
 * as the value read from
 * the module control register.
 * the remainder reflects
 * additional supported modules.
 */

/* XXX - see supported Vik module types in module_vik.c */
#define	VIKING		0x0000		/* Viking without ECache */
#define	VIKING_M	0x0100		/* Viking with MXCC */

#define	ROSS604		0x0010		/* Ross 604-64k module */
#define	ROSS605		0x001F		/* Ross 605-64k module (software) */
#define	ROSS605_A2	0x001F		/* Ross 605-64k module	(rev A2) */
#define	ROSS605_A3	0x001E		/* Ross 605-64k module	(rev A3) */
#define	ROSS605_A4	0x001D		/* Ross 605-64k module	(rev A4) */
#define	ROSS605_A8	0x001B		/* Ross 605-64k module	(rev A8) */
#define	ROSS605_D	0x011F		/* Ross 605-128k module */

#define	IFLUSHNIMP	0		/* Unimplemented flush not handled */
#define	IFLUSHREDOIMP	1		/* Unimplemented flush return -	   */
					/*	redo FLUSH		   */
#define	IFLUSHDONEIMP	2		/* Unimplemented flush return -	   */
					/*	has been handled	   */

#define	TSUNAMI		0x0041		/* Tsunami/Tsupernami */

#define	SWIFT 		0x0004		/* Swift */

/*
 * We can dynamically add or remove support for
 * modules of various sorts by adding them
 * to, or removing them from, this table.
 *
 * The semantics are: VERY VERY early in the
 * execution of the kernel the identify_func
 * are called in sequence. The first that
 * returns non-zero identifies the current
 * module and the specified setup_func is called.
 */

struct module_linkage {
	int 	(*identify_func)();
	void 	(*setup_func)();
};

/*
 * So we can see the "module_info" table
 * where we need it. The table itsself
 * is allocated and filled in the file
 * module_conf.c
 * which is available in binary configurations
 * so "module drivers" may be added.
 */
extern struct module_linkage    module_info[];
extern int module_info_size;

/*
 * The following pointers to functions are staticly
 * initialized to an innocuous "safe" value.
 * In general, MMU related things are set up to
 * do the right thing for the SPARC Reference MMU,
 * and VAC related things are pointed at an
 * empty stub somewhere, but this may change
 * without this header file being updated so
 * go check it out.
 *
 * It is the primary job of the "setup_func"
 * for a module to change these vectors wherever
 * necessary to reference the proper service
 * function for the detected module type.
 */
extern int    (*v_mmu_getcr)();
extern int    (*v_mmu_getctp)();
extern int    (*v_mmu_getctx)();
extern int    (*v_mmu_probe)();
extern void   (*v_mmu_setcr)();
extern void   (*v_mmu_setctp)();
extern void   (*v_mmu_setctxreg)();
extern void   (*v_mmu_flushall)();
extern void   (*v_mmu_flushctx)();
extern void   (*v_mmu_flushrgn)();
extern void   (*v_mmu_flushseg)();
extern void   (*v_mmu_flushpage)();
extern void   (*v_mmu_flushpagectx)();
extern int    (*v_mmu_writepte)();
extern void   (*v_mmu_writeptp)();
extern int    (*v_mp_mmu_writepte)();
extern void   (*v_mmu_getsyncflt)();
extern void   (*v_mmu_getasyncflt)();
extern int    (*v_mmu_chk_wdreset)();
extern int    (*v_mmu_ltic)();
extern int    (*v_mmu_handle_ebe)();
extern void   (*v_mmu_log_module_err)();

extern void   (*v_pac_flushall)();
extern int    (*v_pac_parity_chk_dis)();

extern void   (*v_vac_usrflush)();
extern void   (*v_vac_ctxflush)();
extern void   (*v_vac_rgnflush)();
extern void   (*v_vac_segflush)();
extern void   (*v_vac_pageflush)();
extern void   (*v_vac_flush)();
extern void   (*v_turn_cache_on)();

extern void   (*v_cache_init)();
extern void   (*v_vac_allflush)();

extern void   (*v_uncache_pt_page)();

extern void   (*v_window_overflow)();
extern void   (*v_window_underflow)();
extern void   (*v_mp_window_overflow)();
extern void   (*v_mp_window_underflow)();
extern void   (*v_pac_pageflush)();
extern int    (*v_unimpflush)();	/* IFLUSH */
extern void   (*v_ic_flush)();		/* IFLUSH */

extern int (*v_get_hwcap_flags)(int inkernel);

extern void mp_setfunc(void);
extern int sparcV8_get_hwcap_flags(int);
extern void   (*v_vac_color_sync)();
extern void   (*v_mp_vac_color_sync)();
extern void   (*v_vac_color_flush)();

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_MODULE_H */
