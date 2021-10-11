/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_KD_H
#define	_SYS_KD_H

#pragma ident	"@(#)kd.h	1.20	99/05/04 SMI"

/*
 * Minimal compatibility support for "kd" ioctls.
 *
 * This file may be deleted or changed without notice.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define	KDIOC		('K'<<8)
#define	KDGETMODE	(KDIOC|9)	/* get text/graphics mode */
#define	KDSETMODE	(KDIOC|10)	/* set text/graphics mode */
#define	KD_TEXT		0
#define	KD_GRAPHICS	1

#ifdef __cplusplus
}
#endif

#endif /* _SYS_KD_H */
