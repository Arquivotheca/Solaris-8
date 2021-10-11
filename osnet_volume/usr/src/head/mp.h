/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 */

#ifndef _MP_H
#define	_MP_H

#pragma ident	"@(#)mp.h	1.16	97/07/16 SMI"	/* SVr4.0 1.2	*/

#ifdef	__cplusplus
extern "C" {
#endif

struct mint {
	int len;
	short *val;
};
typedef struct mint MINT;


#ifdef __STDC__
extern void mp_gcd(MINT *, MINT *, MINT *);
extern void mp_madd(MINT *, MINT *, MINT *);
extern void mp_msub(MINT *, MINT *, MINT *);
extern void mp_mdiv(MINT *, MINT *, MINT *, MINT *);
extern void mp_sdiv(MINT *, short, MINT *, short *);
extern int mp_min(MINT *);
extern void mp_mout(MINT *);
extern int mp_msqrt(MINT *, MINT *, MINT *);
extern void mp_mult(MINT *, MINT *, MINT *);
extern void mp_pow(MINT *, MINT *, MINT *, MINT *);
extern void mp_rpow(MINT *, short, MINT *);
extern MINT *mp_itom(short);
extern int mp_mcmp(MINT *, MINT *);
extern MINT *mp_xtom(char *);
extern char *mp_mtox(MINT *);
extern void mp_mfree(MINT *);
#else
extern void mp_gcd();
extern void mp_madd();
extern void mp_msub();
extern void mp_mdiv();
extern void mp_sdiv();
extern int mp_min();
extern void mp_mout();
extern int mp_msqrt();
extern void mp_mult();
extern void mp_pow();
extern void mp_rpow();
extern MINT *mp_itom();
extern int mp_mcmp();
extern MINT *mp_xtom();
extern char *mp_mtox();
extern void mp_mfree();
#endif

#define	FREE(x)	_mp_xfree(&(x))		/* Compatibility */

#ifdef	__cplusplus
}
#endif

#endif /* _MP_H */
