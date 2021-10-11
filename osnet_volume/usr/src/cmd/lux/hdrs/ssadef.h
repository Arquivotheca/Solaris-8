/*
 * Copyright 1999 Sun Microsystems, Inc. All rights reserved.
 */

#ifndef	_SSADEF_H
#define	_SSADEF_H

#pragma ident	"@(#)ssadef.h	1.3	99/07/29 SMI"

/*
 * Include any headers you depend on.
 */

/*
 * I18N message number ranges
 *  This file: 14000 - 14499
 *  Shared common messages: 1 - 1999
 */

#ifdef	__cplusplus
extern "C" {
#endif

#define	S_DPRINTF	if (getenv("SSA_S_DEBUG") != NULL) (void) printf
#define	P_DPRINTF	if (getenv("SSA_P_DEBUG") != NULL) (void) printf
#define	O_DPRINTF	if (getenv("SSA_O_DEBUG") != NULL) (void) printf
#define	I_DPRINTF	if (getenv("SSA_I_DEBUG") != NULL) (void) printf

/*
 * Define for physical name of children of pln
 */
#define	DRV_NAME_SD	"sd@"
#define	DRV_NAME_SSD	"ssd@"
#define	SLSH_DRV_NAME_SD	"/sd@"
#define	SLSH_DRV_NAME_SSD	"/ssd@"


/*
 * format parameter to dump()
 */
#define	HEX_ONLY	0	/* print hex only */
#define	HEX_ASCII	1	/* hex and ascii */


/*
 * 	Status of each drive
 */
#define	NO_LABEL	6	/* The disk label is not a valid UNIX label */
#define	MEDUSA		8	/* Medusa mirrored disk */
#define	DUAL_PORT	9	/* Dual Ported Disk */

#define	P_NPORTS	6
#define	P_NTARGETS	15

#define	P_SCSI_ERROR	0x10000		/* SCSI error */
/* */
#define	P_STRIPE_SIZE	0x20010		/* Invalid stripe block size */
#define	P_GROUP_SIZE	0x20011		/* Invalid number of drives in stripe */
#define	P_GROUP_MEMBERS	0x20012		/* Invalid member in group */
#define	P_DRV_MOUNTED	0x20013		/* drive mounted */
#define	P_DRV_GROUPED	0x20014		/* drive already grouped */
#define	P_NOT_GROUP_DRV	0x20021	/* Trying to ungroup non-grouped drive */
#define	RD_PG21		0x20100		/* Read errro on page 21 */
#define	RD_PG22		0x20101		/* Read error on page 22 */
#define	RD_PG20		0x20102		/* Read error on page 20 */
#define	LOG_PG3d	0x20110	/* Error reading Log Sense page 3d (IOPS) */
#define	LOG_PG3c	0x20111	/* Error reading Log Sense page 3c (IOPS) */
#define	P_INVALID_PATH	0x20200	/* Invalid path name */
#define	P_DOWNLOAD_FILE		0x20300 /* Download file error */
#define	P_DOWNLOAD_CHKSUM	0x20301	/* Invalid download file checksum */
#define	P_NOT_SDA	0x20400		/* Device not SDA */
#define	P_NOT_SUPPORTED	0x20500		/* Function is not supported. */

#ifdef	__cplusplus
}
#endif

#endif	/* _SSADEF_H */
