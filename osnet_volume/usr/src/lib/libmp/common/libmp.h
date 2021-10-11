/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * This is where all the interfaces that are internal to libmp
 * which do not have a better home live
 */

#ifndef _LIBMP_H
#define	_LIBMP_H

#ident	"@(#)libmp.h	1.1	97/06/25 SMI"

#include <mp.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

extern short *_mp_xalloc(int, char *);
extern void _mp_xfree(MINT *);
extern void _mp_move(MINT *, MINT *);
extern void mp_invert(MINT *, MINT *, MINT *);
extern void _mp_fatal(char *);
extern void _mp_mcan(MINT *);
extern char *mtox(MINT *);
extern int mp_omin(MINT *);
extern void mp_omout(MINT *);
extern void mp_fmout(MINT *, FILE *);
extern int mp_fmin(MINT *, FILE *);

/*
 * old libmp interfaces
 */
extern void gcd(MINT *, MINT *, MINT *);
extern void madd(MINT *, MINT *, MINT *);
extern void msub(MINT *, MINT *, MINT *);
extern void mdiv(MINT *, MINT *, MINT *, MINT *);
extern void sdiv(MINT *, short, MINT *, short *);
extern int min(MINT *);
extern void mout(MINT *);
extern int msqrt(MINT *, MINT *, MINT *);
extern void mult(MINT *, MINT *, MINT *);
extern void pow(MINT *, MINT *, MINT *, MINT *);
extern void rpow(MINT *, short, MINT *);
extern MINT *itom(short);
extern int mcmp(MINT *, MINT *);
extern MINT *xtom(char *);
extern char *mtox(MINT *);
extern void mfree(MINT *);
extern short *xalloc(int, char *);
extern void xfree(MINT *);

#ifdef __cplusplus
}
#endif

#endif /* _LIBMP_H */
