/*
 * Copyright (c) 1995-1996, Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef	_SYS_DOSERR_H
#define	_SYS_DOSERR_H

#pragma ident	"@(#)doserr.h	1.2	96/01/11 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  Failure codes returned by MSDOS when various int 21 functions fail.
 */
#define	DOSERR_INVALIDFCN	0x01
#define	DOSERR_FILENOTFOUND	0x02
#define	DOSERR_PATHNOTFOUND	0x03
#define	DOSERR_ACCESSDENIED	0x05
#define	DOSERR_INVALIDHANDLE	0x06
#define	DOSERR_INSUFFICIENT_MEMORY	0x08
#define	DOSERR_MEMBLK_ADDR_BAD	0x09
#define	DOSERR_NOMOREFILES	0x12
#define	DOSERR_SEEKERROR	0x19

#ifdef __cplusplus
}
#endif

#endif /* _SYS_DOSERR_H */
