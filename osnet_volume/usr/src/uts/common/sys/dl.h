/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_DL_H
#define	_SYS_DL_H

#pragma ident	"@(#)dl.h	1.15	97/04/25 SMI"	/* SVr4.0 1.3	*/

#include <sys/isa_defs.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef	struct dl {
#ifdef _LONG_LONG_LTOH
	uint_t	dl_lop;
	int	dl_hop;
#else
	int	dl_hop;
	uint_t	dl_lop;
#endif
} dl_t;

#ifdef __STDC__
extern dl_t	ladd(dl_t, dl_t);
extern dl_t	lsub(dl_t, dl_t);
extern dl_t	lmul(dl_t, dl_t);
extern dl_t	ldivide(dl_t, dl_t);
extern dl_t	lshiftl(dl_t, int);
extern dl_t	llog10(dl_t);
extern dl_t	lexp10(dl_t);
#else
extern dl_t	ladd();
extern dl_t	lsub();
extern dl_t	lmul();
extern dl_t	ldivide();
extern dl_t	lshiftl();
extern dl_t	llog10();
extern dl_t	lexp10();
#endif	/* __STDC__ */

extern dl_t	lzero;
extern dl_t	lone;
extern dl_t	lten;

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DL_H */
