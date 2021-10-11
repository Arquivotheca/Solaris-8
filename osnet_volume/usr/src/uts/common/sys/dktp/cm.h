/*
 * Copyright (c) 1992 Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef _SYS_DKTP_CM_H
#define	_SYS_DKTP_CM_H

#pragma ident	"@(#)cm.h	1.12	94/09/03 SMI"

#include <sys/types.h>
#ifdef	_KERNEL
#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/fcntl.h>
#include <sys/open.h>
#include <sys/sysmacros.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#endif	/* _KERNEL */

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_KERNEL

#ifndef _SYS_SCSI_SCSI_H
#ifdef	__STDC__
typedef	void *	opaque_t;
#else	/* __STDC__ */
typedef	char *	opaque_t;
#endif	/* __STDC__ */
#endif

#define	PRF		prom_printf

#define	SET_BP_SEC(bp, X) ((bp)->b_private = (void *) (X))
#define	GET_BP_SEC(bp) ((daddr_t)(bp)->b_private)

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DKTP_CM_H */
