/*
 * Copyright (c) 1986, 1990 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef	_SYS_BW2VAR_H
#define	_SYS_BW2VAR_H

#pragma ident	"@(#)bw2var.h	1.17	92/07/14 SMI"	/* SunOS-4.0 1.13 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * bw2 -- monochrome frame buffer
 */

/* standard resolution */
#define	BW2SIZEX 1152
#define	BW2SIZEY 900
#define	BW2BYTES (BW2SIZEX*BW2SIZEY/8)

#define	BW2SQUARESIZEX 1024
#define	BW2SQUARESIZEY 1024
#define	BW2SQUAREBYTES (BW2SQUARESIZEX*BW2SQUARESIZEY/8)

/* high resolution (bw2h) */
#define	BW2HSIZEX	1600
#define	BW2HSIZEY	1280
#define	BW2HBYTES	(BW2HSIZEX*BW2HSIZEY/8)

#define	BW2HSQUARESIZEX 1440
#define	BW2HSQUARESIZEY 1440
#define	BW2HSQUAREBYTES (BW2HSQUARESIZEX*BW2HSQUARESIZEY/8)


extern	struct pixrectops bw2_ops;

#ifndef _KERNEL
struct	pixrect *bw2_make();
int	bw2_destroy();
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_BW2VAR_H */
