/*
 * Copyright (c) 1992 Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef _SYS_DKTP_CMDEV_H
#define	_SYS_DKTP_CMDEV_H

#pragma ident	"@(#)cmdev.h	1.4	94/09/03 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	CMDEV_TARG(devp)	(devp)->sd_address.a_target
#define	CMDEV_LUN(devp)		(devp)->sd_address.a_lun

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_CMDEV_H */
