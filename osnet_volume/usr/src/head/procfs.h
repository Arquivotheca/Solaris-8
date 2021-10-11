/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_PROCFS_H
#define	_PROCFS_H

#pragma ident	"@(#)procfs.h	1.2	96/06/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This include file forces the new structured /proc definitions.
 * The structured /proc interface is the preferred API, and the
 * older ioctl()-based /proc interface will be removed in a future
 * version of Solaris.
 */
#ifdef	_STRUCTURED_PROC
#undef	_STRUCTURED_PROC
#endif
#define	_STRUCTURED_PROC	1

#include <sys/procfs.h>

/*
 * libproc API
 */

#ifdef	__cplusplus
}
#endif

#endif	/* _PROCFS_H */
