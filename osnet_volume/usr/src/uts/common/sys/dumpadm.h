/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_DUMPADM_H
#define	_SYS_DUMPADM_H

#pragma ident	"@(#)dumpadm.h	1.4	98/06/03 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * ioctl commands for /dev/dump
 */
#define	DDIOC		(0xdd << 8)
#define	DIOCGETDUMPSIZE	(DDIOC | 0x10)
#define	DIOCGETCONF	(DDIOC | 0x11)
#define	DIOCSETCONF	(DDIOC | 0x12)
#define	DIOCGETDEV	(DDIOC | 0x13)
#define	DIOCSETDEV	(DDIOC | 0x14)
#define	DIOCTRYDEV	(DDIOC | 0x15)
#define	DIOCDUMP	(DDIOC | 0x16)

/*
 * Kernel-controlled dump state flags for dump_conflags
 */
#define	DUMP_EXCL	0x00000001	/* dedicated dump device (not swap) */
#define	DUMP_STATE	0x0000ffff	/* the set of all kernel flags */

/*
 * User-controlled dump content flags (mutually exclusive) for dump_conflags
 */
#define	DUMP_KERNEL	0x00010000	/* dump kernel pages only */
#define	DUMP_ALL	0x00020000	/* dump all pages */
#define	DUMP_CONTENT	0xffff0000	/* the set of all dump content flags */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DUMPADM_H */
