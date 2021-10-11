/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_FIBRE_CHANNEL_ULP_SSFCP_VAR_H
#define	_SYS_FIBRE_CHANNEL_ULP_SSFCP_VAR_H

#pragma ident	"@(#)fcp_util.h	1.1	99/07/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>


#define	FCP_TGT_INQUIRY		0x01
#define	FCP_TGT_CREATE		0x02
#define	FCP_TGT_DELETE		0x04

struct	fcp_ioctl {
	minor_t		fp_minor;
	uint32_t	listlen;
	caddr_t		list;
};

struct	device_data {
	la_wwn_t	dev_pwwn;
	int		dev_status;
	int		dev_lun_cnt;
	uchar_t		dev0_type;
};

#if defined(_SYSCALL32)
/*
 * 32 bit varient of fcp_ioctl
 * used only in the driver.
 */

struct	fcp32_ioctl {
	minor_t		fp_minor;
	uint32_t	listlen;
	caddr32_t	list;
};

#endif /* _SYSCALL32 */
#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FIBRE_CHANNEL_ULP_SSFCP_VAR_H */
