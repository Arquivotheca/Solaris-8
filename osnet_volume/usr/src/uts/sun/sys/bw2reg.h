/*
 * Copyright 1985, 1987, 1990 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef	_SYS_BW2REG_H
#define	_SYS_BW2REG_H

#pragma ident	"@(#)bw2reg.h	1.19	93/04/22 SMI"	/* SunOS-4.0 1.14 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Monochrome memory frame buffer hardware definitions
 */

#define	BW2_FBSIZE		(128*1024)	/* size of frame buffer */
#define	BW2_FBSIZE_HIRES	(256*1024)	/* hi-res frame buffer size */

#define	BW2_USECOPYMEM		0x1	/* config flag to use copy memory */

#ifdef _KERNEL

#define	BW2_COPY_MEM_AVAIL	(defined(sun2) || defined(SUN3_160))

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_BW2REG_H */
