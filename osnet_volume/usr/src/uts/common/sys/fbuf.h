/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_FBUF_H
#define	_SYS_FBUF_H

#pragma ident	"@(#)fbuf.h	1.13	98/01/06 SMI"	/* SVr4.0 1.3	*/

#include <sys/vnode.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * A struct fbuf is used to get a mapping to part of a file using the
 * segkmap facilities.  After you get a mapping, you can fbrelse() it
 * (giving a seg code to pass back to segmap_release), you can fbwrite()
 * it (causes a synchronous write back using the file mapping information),
 * or you can fbiwrite it (causing indirect synchronous write back to
 * the block number given without using the file mapping information).
 */

struct fbuf {
	caddr_t	fb_addr;
	uint_t	fb_count;
};

#if defined(__STDC__)
extern int fbread(struct vnode *, offset_t, uint_t, enum seg_rw,
    struct fbuf **);
extern void fbzero(struct vnode *, offset_t, uint_t, struct fbuf **);
extern int fbwrite(struct fbuf *);
extern int fbdwrite(struct fbuf *);
extern int fbiwrite(struct fbuf *, struct vnode *, daddr_t bn, int bsize);
extern void fbrelse(struct fbuf *, enum seg_rw);
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FBUF_H */
