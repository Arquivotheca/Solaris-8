/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Common definitions
 */

#ifndef	_L_COMMON
#define	_L_COMMON

#pragma ident	"@(#)l_common.h	1.7	99/07/22 SMI"

/*
 * Include any headers you depend on.
 */

/*
 * I18N message number ranges
 *  This file: 14500 - 14999
 *  Shared common messages: 1 - 1999
 */

#ifdef	__cplusplus
extern "C" {
#endif

#include	<sys/scsi/targets/sesio.h>

/*
 * Debug environmental flags.
 */
/* SCSI Commands */
#define	S_DPRINTF	if (getenv("_LUX_S_DEBUG") != NULL) (void) printf

/* General purpose */
#define	P_DPRINTF	if (getenv("_LUX_P_DEBUG") != NULL) (void) printf

/* Opens */
#define	O_DPRINTF	if (getenv("_LUX_O_DEBUG") != NULL) (void) printf

/* Ioctls */
#define	I_DPRINTF	if (getenv("_LUX_I_DEBUG") != NULL) (void) printf

/* Hot-Plug */
#define	H_DPRINTF	if (getenv("_LUX_H_DEBUG") != NULL) (void) printf

/* Convert Name debug variable. */
#define	L_DPRINTF	if (getenv("_LUX_L_DEBUG") != NULL) (void) printf

/* Getting status */
#define	G_DPRINTF	if (getenv("_LUX_G_DEBUG") != NULL) (void) printf

/* Box list */
#define	B_DPRINTF	if (getenv("_LUX_B_DEBUG") != NULL) (void) printf

/* Non-Photon disks */
#define	N_DPRINTF	if (getenv("_LUX_N_DEBUG") != NULL) (void) printf

/* Null WWN FCdisks */
#define	W_DPRINTF	if (getenv("_LUX_W_DEBUG") != NULL) (void) printf

/* Devices */
#define	D_DPRINTF	if (getenv("_LUX_D_DEBUG") != NULL) (void) printf

/* Enable/Bypass */
#define	E_DPRINTF	if (getenv("_LUX_E_DEBUG") != NULL) (void) printf

/* Standard Error messages. */
#define	ER_DPRINTF	if (getenv("_LUX_ER_DEBUG") != NULL) (void) printf

/* Retries */
#define	R_DPRINTF	if (getenv("_LUX_R_DEBUG") != NULL) (void) printf

/* Threads & Timing */
#define	T_DPRINTF	if (getenv("_LUX_T_DEBUG") != NULL) (void) printf

/* Allocation */
#define	A_DPRINTF	if (getenv("_LUX_A_DEBUG") != NULL) (void) printf




/* Warning messages */
#define	L_WARNINGS	if (getenv("_LUX_WARNINGS") != NULL) (void) printf


#define	MIN(a, b)		(a < b ? a : b)

/*
 * Define for physical name of children of fcp
 */
#define	FC_CTLR			":devctl"
#define	DRV_NAME_SD		"sd@"
#define	DRV_NAME_SSD		"ssd@"
#define	SLSH_DRV_NAME_SD	"/sd@"
#define	SLSH_DRV_NAME_SSD	"/ssd@"
#define	DRV_PART_NAME		",0:c,raw"
#define	SES_NAME		"ses@"
#define	SLSH_SES_NAME		"/ses@"
#define	SLASH_SES		"/ses"
#define	SES_DIR			"/dev/es"
#define	DEV_DIR			"/dev/dsk"
#define	DEV_RDIR		"/dev/rdsk"
#define	L_ARCH_4M		"sun4m"
#define	L_ARCH_4D		"sun4d"

/*
 * format parameter to dump()
 */
#define	HEX_ONLY	0	/* print hex only */
#define	HEX_ASCII	1	/* hex and ascii */

/*
 * controller/nexus node postfix strings
 */
#define	CTLR_POSTFIX	":ctlr"
#define	DEVCTL_POSTFIX	":devctl"


#ifdef	__cplusplus
}
#endif

#endif	/* _L_COMMON */
