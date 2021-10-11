/*
 * Copyright (c) 1995-1996, Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef	_SYS_BOOTCMN_H
#define	_SYS_BOOTCMN_H

#pragma ident	"@(#)bootcmn.h	1.2	96/01/11 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*	dummy device names for boot and floppy devices			*/
#define	BOOT_DEV_NAME	"<bootdev>"
#define	FLOPPY0_NAME	"/dev/diskette0"
#define	FLOPPY1_NAME	"/dev/diskette1"

/*	Maximum size (in characters) allotted to DOS volume labels	*/
#define	VOLLABELSIZE	11

#ifdef __cplusplus
}
#endif

#endif /* _SYS_BOOTCMN_H */
