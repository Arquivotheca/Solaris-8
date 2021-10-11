
/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

/*
 * PLUTO CONFIGURATION MANAGER
 * Error definitions
 */

#ifndef	_P_ERROR
#define	_P_ERROR

#pragma ident	"@(#)error.h	1.2	95/11/21 SMI"

/*
 * Include any headers you depend on.
 */

#ifdef	__cplusplus
extern "C" {
#endif

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

#endif	/* _P_ERROR */
