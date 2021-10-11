/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_CLADM_H
#define	_SYS_CLADM_H

#pragma ident	"@(#)cladm.h	1.3	99/08/18 SMI"

#include <sys/types.h>
#include <sys/clconf.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This file defines interfaces which are private to Sun Clustering.
 * Others should not depend on this in any way as it may change or be
 * removed completely.
 */

/*
 * cladm() facilities; see below for definitions pertinent to each of these
 * facilities.
 */
#define	CL_INITIALIZE		0	/* bootstrapping information */
#define	CL_CONFIG		1	/* configuration information */


/*
 * Command definitions for each of the facilities.
 * The type of the data pointer and the direction of the data transfer
 * is listed for each command.
 */

/*
 * CL_INITIALIZE facility commands.
 */
#define	CL_GET_BOOTFLAG		0	/* Return cluster config/boot status */

/*
 * Definitions for the flag bits returned by CL_GET_BOOTFLAG.
 */
#define	CLUSTER_CONFIGURED	0x0001	/* system is configured as a cluster */
#define	CLUSTER_BOOTED		0x0002	/* system is booted as a cluster */

#ifdef _KERNEL
#define	CLUSTER_INSTALLING	0x0004	/* cluster is being installed */
#define	CLUSTER_DCS_ENABLED	0x0008	/* cluster device framework enabled */
#endif	/* _KERNEL */

/*
 * CL_CONFIG facility commands.
 */
#define	CL_NODEID		0	/* Return nodeid of this node. */
#define	CL_HIGHEST_NODEID	1	/* Return highest configured nodeid. */
#define	CL_GDEV_PREFIX		2	/* Return path to global namespace. */

#ifdef _KERNEL
extern int cladmin(int fac, int cmd, void *data);
extern int cluster_bootflags;
#else
#if defined(__STDC__)
extern int _cladm(int fac, int cmd, void *data);
#else	/* !defined(__STDC__) */
extern int _cladm();
#endif	/* defined(__STDC__) */
#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CLADM_H */
