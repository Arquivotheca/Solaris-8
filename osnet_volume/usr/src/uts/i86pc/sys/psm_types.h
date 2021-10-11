/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PSM_TYPES_H
#define	_SYS_PSM_TYPES_H

#pragma ident	"@(#)psm_types.h	1.10	98/01/06 SMI"

/*
 * Platform Specific Module Types
 */

#include <sys/types.h>
#include <sys/cpuvar.h>
#include <sys/time.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct 	psm_ops {
	int	(*psm_probe)(void);

	void	(*psm_softinit)(void);
	void	(*psm_picinit)(void);
	int	(*psm_intr_enter)(int ipl, int *vectorp);
	void	(*psm_intr_exit)(int ipl, int irqno);
	void	(*psm_setspl)(int ipl);
	int	(*psm_addspl)(int irqno, int ipl, int min_ipl, int max_ipl);
	int	(*psm_delspl)(int irqno, int ipl, int min_ipl, int max_ipl);
	int	(*psm_disable_intr)(processorid_t cpun);
	void	(*psm_enable_intr)(processorid_t cpun);
	int	(*psm_softlvl_to_irq)(int ipl);
	void	(*psm_set_softintr)(int ipl);
	void	(*psm_set_idlecpu)(processorid_t cpun);
	void	(*psm_unset_idlecpu)(processorid_t cpun);

	void	(*psm_clkinit)(int hertz);
	int	(*psm_get_clockirq)(int ipl);
	void	(*psm_hrtimeinit)(void);
	hrtime_t (*psm_gethrtime)(void);

	processorid_t (*psm_get_next_processorid)(processorid_t cpu_id);
	void	(*psm_cpu_start)(processorid_t cpun, caddr_t rm_code);
	int	(*psm_post_cpu_start)(void);
#if defined(PSMI_1_2)
	void	(*psm_shutdown)(int cmd, int fcn);
#else
	void	(*psm_shutdown)(void);
#endif
	int	(*psm_get_ipivect)(int ipl, int type);
	void	(*psm_send_ipi)(processorid_t cpun, int ipl);

	int	(*psm_translate_irq)(dev_info_t *dip, int irqno);

	int	(*psm_tod_get)(todinfo_t *tod);
	int	(*psm_tod_set)(todinfo_t *tod);

	void	(*psm_notify_error)(int level, char *errmsg);
#if defined(PSMI_1_2)
	void	(*psm_notify_func)(int msg);
#endif
};


struct 	psm_info {
	ushort_t p_version;
	ushort_t p_owner;
	struct 	psm_ops	*p_ops;
	char	*p_mach_idstring;	/* machine identification string */
	char	*p_mach_desc;		/* machine descriptions		 */
};

/*
 * version
 * 0x86vm where v = (version no. - 1) and m = (minor no. + 1)
 * i.e. psmi 1.0 has v=0 and m=1, psmi 1.1 has v=0 and m=2
 * also, 0x86 in the high byte is the signature of the psmi
 */
#define	PSM_INFO_VER01		0x8601
#define	PSM_INFO_VER01_1	0x8602
#define	PSM_INFO_VER01_2	0x8603
#define	PSM_INFO_VER01_X	(PSM_INFO_VER01_1 & 0xFFF0)	/* ver 1.X */

/*
 *	owner field definitions
 */
#define	PSM_OWN_SYS_DEFAULT	0x0001
#define	PSM_OWN_EXCLUSIVE	0x0002
#define	PSM_OWN_OVERRIDE	0x0003

#define	PSM_NULL_INFO		-1

/*
 *	Arg to psm_notify_func
 */
#define	PSM_DEBUG_ENTER		1
#define	PSM_DEBUG_EXIT		2
#define	PSM_PANIC_ENTER		3

/*
 *	Soft-level to interrupt vector
 */
#define	PSM_SV_SOFTWARE		-1
#define	PSM_SV_MIXED		-2

/*
 *	Inter-processor interrupt type
 */
#define	PSM_INTR_IPI_HI		0x01
#define	PSM_INTR_IPI_LO		0x02
#define	PSM_INTR_POKE		0x03

/*
 *	return code
 */
#define	PSM_SUCCESS		DDI_SUCCESS
#define	PSM_FAILURE		DDI_FAILURE

#define	PSM_INVALID_IPL		0
#define	PSM_INVALID_CPU		-1

#define	PSM_KVTOP(vaddr) \
	((paddr_t)(hat_getkpfnum((caddr_t)(vaddr)) << (PAGESHIFT)) | \
	    ((paddr_t)(vaddr) & (PAGEOFFSET)))

struct 	psm_ops_ver01 {
	int	(*psm_probe)(void);

	void	(*psm_softinit)(void);
	void	(*psm_picinit)(void);
	int	(*psm_intr_enter)(int ipl, int *vectorp);
	void	(*psm_intr_exit)(int ipl, int irqno);
	void	(*psm_setspl)(int ipl);
	int	(*psm_addspl)(int irqno, int ipl, int min_ipl, int max_ipl);
	int	(*psm_delspl)(int irqno, int ipl, int min_ipl, int max_ipl);
	int	(*psm_disable_intr)(processorid_t cpun);
	void	(*psm_enable_intr)(processorid_t cpun);
	int	(*psm_softlvl_to_irq)(int ipl);
	void	(*psm_set_softintr)(int ipl);
	void	(*psm_set_idlecpu)(processorid_t cpun);
	void	(*psm_unset_idlecpu)(processorid_t cpun);

	void	(*psm_clkinit)(int hertz);
	int	(*psm_get_clockirq)(int ipl);
	void	(*psm_hrtimeinit)(void);
	hrtime_t (*psm_gethrtime)(void);

	processorid_t (*psm_get_next_processorid)(processorid_t cpu_id);
	void	(*psm_cpu_start)(processorid_t cpun, caddr_t rm_code);
	int	(*psm_post_cpu_start)(void);
	void	(*psm_shutdown)(void);
	int	(*psm_get_ipivect)(int ipl, int type);
	void	(*psm_send_ipi)(processorid_t cpun, int ipl);
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PSM_TYPES_H */
