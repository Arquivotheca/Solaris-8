/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_CPR_IMPL_H
#define	_SYS_CPR_IMPL_H

#pragma ident	"@(#)cpr_impl.h	1.21	98/10/29 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This file contains machine dependent information for CPR
 */
#define	CPR_MACHTYPE_4M		0x346d		/* '4m' */
#define	CPRBOOT			"cprboot"

/*
 * sun4m info that needs to be saved in the state file for resume.
 */
struct cpr_sun4m_machdep {
	uint_t	func;
	uint_t	func_pfn;
	uint_t	thrp;
	uint_t	thrp_pfn;
	uint_t	qsav;
	uint_t	qsav_pfn;
	uint_t	mmu_ctp;
	uint_t	mmu_ctx;
	uint_t	mmu_ctl;
};
typedef struct cpr_sun4m_machdep csm_md_t;

#define	PATOPTP_SHIFT	4
#define	PN_TO_ADDR(pn)	(((pn) << MMU_STD_PAGESHIFT) & MMU_STD_PAGEMASK)
#define	ADDR_TO_PN(a)	(((a) >> MMU_STD_PAGESHIFT) & MMU_STD_ROOTMASK)

typedef uint32_t cpr_ptr;
typedef uint32_t cpr_ext;
typedef	ulong_t physaddr_t;

#define	prom_map_plat(addr, pa, size) \
	if (prom_map(addr, 0, pa, size) == 0) { \
		errp("PROM_MAP failed: paddr=0x%x\n", pa); \
		return (-1); \
	}

extern int i_cpr_check_cprinfo(void);
extern int i_cpr_prom_pages(int);
extern int i_cpr_reuseinit(void);
extern int i_cpr_singleuse(void);
extern int i_cpr_write_machdep(vnode_t *);
extern void i_cpr_enable_intr(void);
extern void i_cpr_handle_xc(uint_t);
extern void i_cpr_machdep_setup(void);
extern void i_cpr_resume_setup(uint_t, uint_t, caddr_t, caddr_t, uint_t);
extern void i_cpr_save_machdep_info(void);
extern void i_cpr_set_tbr(void);
extern void i_cpr_stop_intr(void);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CPR_IMPL_H */
