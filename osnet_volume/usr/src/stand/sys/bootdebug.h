/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef	_SYS_BOOTDEBUG_H
#define	_SYS_BOOTDEBUG_H

#pragma ident	"@(#)bootdebug.h	1.2	96/06/06 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * a collection of usefule debug defines and macros
 */

/* #define	COMPFS_OPS_DEBUG */
/* #define	PCFS_OPS_DEBUG */
/* #define	HSFS_OPS_DEBUG */
/* #define	UFS_OPS_DEBUG */
/* #define	NFS_OPS_DEBUG */
/* #define	CFS_OPS_DEBUG */
/* #define	VERIFY_HASH_REALLOC */

#include <sys/reboot.h>

extern int boothowto;			/* What boot options are set */
extern int verbosemode;
#define	DBFLAGS	(RB_DEBUG | RB_VERBOSE)

/*
 * Debug Message Macros - will print message if CFS_OPS_DEBUG
 * is defined.
 */
#ifdef CFS_OPS_DEBUG

#define	OPS_DEBUG(args)	{ printf args; }
#define	OPS_DEBUG_CK(args)\
	{ if ((boothowto & DBFLAGS) == DBFLAGS) printf args; }

#else CFS_OPS_DEBUG

#define	OPS_DEBUG(args)	/* nothing */
#define	OPS_DEBUG_CK(args)	/* nothing */

#endif CFS_OPS_DEBUG

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_BOOTDEBUG_H */
