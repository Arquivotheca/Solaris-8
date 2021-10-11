/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_DKTP_CMDK_H
#define	_SYS_DKTP_CMDK_H

#pragma ident	"@(#)cmdk.h	1.11	99/03/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct	dk_openinfo {
	ulong_t  dk_reg[OTYPCNT];
	ulong_t  dk_exl[OTYPCNT];
};

struct	cmdk_label {
	opaque_t	dkl_objp;
	char		dkl_name[OBJNAMELEN];
};

#define	CMDK_LABEL_MAX	3
struct	cmdk {
	long		dk_flag;
	dev_info_t	*dk_dip;
	dev_t		dk_dev;

	ksema_t		dk_semoclose;	/* lock for opens/closes 	*/
	ulong_t  	*dk_lyr;
	struct		dk_openinfo dk_open;

	opaque_t 	dk_tgobjp;	/* target disk object pointer	*/
	opaque_t 	dk_lbobjp;
	struct cmdk_label dk_lb[CMDK_LABEL_MAX];

	kmutex_t	dk_pinfo_lock;
	kcondvar_t	dk_pinfo_cv;
	int		dk_pinfo_state;
};

/*	common disk flags definitions					*/
#define	CMDK_OPEN		0x1
#define	CMDK_VALID_LABEL	0x2

#define	CMDK_UNITSHF	6
#define	CMDK_MAXPART	(1 << CMDK_UNITSHF)

#define	CMDKUNIT(dev) (getminor((dev)) >> CMDK_UNITSHF)
#define	CMDKPART(dev) (getminor((dev)) & (CMDK_MAXPART - 1))

#define	CMDK_TGOBJP(dkp)	(dkp)->dk_tgobjp

/*	dk_pinfo_states for cmdk_part_info() */
#define	CMDK_PARTINFO_INVALID	0
#define	CMDK_PARTINFO_BUSY	1
#define	CMDK_PARTINFO_BUSY2	2
#define	CMDK_PARTINFO_VALID	3


#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_CMDK_H */
