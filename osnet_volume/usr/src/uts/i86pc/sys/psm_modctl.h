/*
 * Copyright (c) 1993, by Sun Microsystems, Inc.
 */

#ifndef	_SYS_PSM_MODCTL_H
#define	_SYS_PSM_MODCTL_H

#pragma ident	"@(#)psm_modctl.h	1.2	93/11/15 SMI"

/*
 * loadable module support.
 */

#ifdef	__cplusplus
extern "C" {
#endif

struct psm_sw {
	struct psm_sw	*psw_forw;
	struct psm_sw	*psw_back;
	struct psm_info *psw_infop;
	int	psw_flag;
};

#define	PSM_MOD_INSTALL		0x0001
#define	PSM_MOD_IDENTIFY	0x0002

/* For psm */
struct modlpsm {
	struct mod_ops		*psm_modops;
	char			*psm_linkinfo;
	struct psm_sw		*psm_swp;
};

extern struct psm_sw *psmsw;
extern kmutex_t psmsw_lock;

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PSM_MODCTL_H */
