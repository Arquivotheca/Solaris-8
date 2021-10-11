/*	@(#)libm.h	1.1	92/04/17 SMI	*/

/*
 * Copyright (C) 1990 by Sun Microsystems, Inc.
 */

#define TRUE  1
#define FALSE 0

#ifdef _ASM

#ifdef ELFOBJ

#ifndef	i386
#include <sys/asm_linkage.h>
#endif	/* i386 */

#define NAME(x) x
#define TEXT	.section	".text"
#define DATA	.section	".data"
#define IDENT(x)	.ident	x

#ifndef	i386
#define	LIBM_ANSI_PRAGMA_WEAK(sym,stype) \
	.weak sym; \
	.type sym,#stype; \
sym	= __/**/sym

#ifndef SET_FILE
#define	SET_FILE(x) \
	.file	x
#endif	/* !defined(SET_FILE) */

#else	/* i386 */
#define	LIBM_ANSI_PRAGMA_WEAK(sym,stype) \
	.weak sym; \
	.type sym,@stype; \
sym	= __/**/sym
#endif	/* i386 */

#else /* ifdef ELFOBJ */

#include <machine/asm_linkage.h>

#ifndef	SET_SIZE
#define	SET_SIZE(x)
#endif	/* !defined(SET_SIZE) */

#ifndef SET_FILE
#define	SET_FILE(x)
#endif	/* !defined(SET_FILE) */

#define TEXT	.seg	"text"
#define DATA	.seg	"data"
#define IDENT(x)	.seg "data" ; .asciz	x
#define	LIBM_ANSI_PRAGMA_WEAK(sym,stype)
#define	ANSI_PRAGMA_WEAK(sym,stype)

#endif /* ifdef ELFOBJ */

#else /* ifdef _ASM */

extern double _SVID_libm_err(double, double, int);
#ifndef SUNSOFT
extern float _SVID_libm_errf(float, float, int);
#endif

#define XTOI(E,I)	\
		I = ((E[2]<<16) | (0xffff & (E[1]>>15)))
#define ITOX(I,E)       \
		E[2] = 0xffff & (I>>16) ; \
		E[1] = ((I & 0x7fffffff) == 0) \
			? (E[1] & 0x7fff) | (0x7fff8000 & (I<<15)) \
			: 0x80000000 | (E[1] & 0x7fff) | (0x7fff8000 & (I<<15))

/* 
 * IEEE double precision constants 
 * Include "libm_cdefs.h" then set _LIBM_CONSTANTS to the desired selection.
 */
#ifdef _LIBM_CONSTANTS

typedef const union {
	unsigned long ul[2];
	double d;
} union_ul_d;

#ifndef	i386

typedef const union {
	unsigned long ul[4];
	long double ld;
} union_ul_ld;

#if _LIBM_CONSTANTS&_D_LN2
static union_ul_d ln2x =	{ 0x3fe62e42, 0xfefa39ef };
#endif
#if _LIBM_CONSTANTS&_D_LN2HI
static union_ul_d ln2hix =	{ 0x3fe62e42, 0xfee00000 };
static union_ul_d ln2lox =	{ 0x3dea39ef, 0x35793c76 };
#endif
#if _LIBM_CONSTANTS&_D_NAN
static union_ul_d NaNx = 	{ 0x7fffffff, 0xffffffff };
#endif
#if _LIBM_CONSTANTS&_D_INF
static union_ul_d Infx = 	{ 0x7ff00000, 0x00000000 };
#endif
#if _LIBM_CONSTANTS&_D_TWO52
static union_ul_d two52x =	{ 0x43300000, 0x00000000 };
#endif
#if _LIBM_CONSTANTS&_D_TWOM52
static union_ul_d twom52x =	{ 0x3cb00000, 0x00000000 };
#endif
#if _LIBM_CONSTANTS&_D_FMAX
static union_ul_d fmaxx =	{ 0x7fefffff, 0xffffffff };
#endif
#if _LIBM_CONSTANTS&_D_FMIN
static union_ul_d fminx =	{ 0x00000000, 0x00000001 };
#endif
#if _LIBM_CONSTANTS&_D_SNAN
static union_ul_d sNaNx =	{ 0x7ff00000, 0x00000001 };
static union_ul_d fmaxsx =	{ 0x000fffff, 0xffffffff };
static union_ul_d fminnx =	{ 0x00100000, 0x00000000 };
#endif
#if _LIBM_CONSTANTS&_D_LNOVFT
static union_ul_d lnovftx =	{ 0x40862e42, 0xfefa39ef };/* chopped */
#endif
#if _LIBM_CONSTANTS&_D_LNUNFT
static union_ul_d lnunftx =	{ 0xc0874910, 0xd52d3052 };/* ln(minf/2) chop*/
#endif
#if _LIBM_CONSTANTS&_D_PI_RZ
static union_ul_d PI_RZx =	{ 0x400921fb, 0x54442d18 };
#endif
#if _LIBM_CONSTANTS&_D_INVLN2
static union_ul_d invln2x =	{ 0x3ff71547, 0x652b82fe };
#endif
#if _LIBM_CONSTANTS&_D_SQRT2
static union_ul_d sqrt2x =	{ 0x3ff6a09e, 0x667f3bcd };/* rounded up */
#endif
#if _LIBM_CONSTANTS&_D_SQRT2P1
static union_ul_d sqrt2p1_hix =	{ 0x4003504f, 0x333f9de6 };
static union_ul_d sqrt2p1_lox =	{ 0x3ca21165, 0xf626cdd5 };
#endif
#if _LIBM_CONSTANTS&_D_INFL
static union_ul_ld inflx  = {0x7fff0000, 0x00000000, 0x00000000, 0x00000000};
static union_ul_ld minnlx = {0x00010000, 0x00000000, 0x00000000, 0x00000000};
static union_ul_ld minslx = {0x00000000, 0x00000000, 0x00000000, 0x00000001};
static union_ul_ld maxlx  = {0x7ffeffff, 0xffffffff, 0xffffffff, 0xffffffff};
static union_ul_ld maxslx = {0x0000ffff, 0xffffffff, 0xffffffff, 0xffffffff};
static union_ul_ld NaNlx  = {0x7fff8000, 0x00000000, 0x00000000, 0x00000000};
static union_ul_ld sNaNlx = {0x7fff0000, 0x00000000, 0x00000000, 0x00000001};
#endif

#else	/* i386 */

typedef const union {
	unsigned long ul[3];
	long double ld;
} union_ul_ld;

static union_ul_d ln2x =	{ 0xfefa39ef, 0x3fe62e42 };
static union_ul_d ln2hix =	{ 0xfee00000, 0x3fe62e42 };
static union_ul_d ln2lox =	{ 0x35793c76, 0x3dea39ef };
static union_ul_d NaNx =	{ 0xffffffff, 0x7fffffff };
static union_ul_d Infx =	{ 0x00000000, 0x7ff00000 };
static union_ul_d two52x =	{ 0x00000000, 0x43300000 };
static union_ul_d twom52x =	{ 0x00000000, 0x3cb00000 };
static union_ul_d fmaxx =	{ 0xffffffff, 0x7fefffff };
static union_ul_d fminx =	{ 0x00000001, 0x00000000 };
static union_ul_d sNaNx =	{ 0x00000001, 0x7ff00000 };
static union_ul_d fmaxsx =	{ 0xffffffff, 0x000fffff };
static union_ul_d fminnx =	{ 0x00000000, 0x00100000 };
static union_ul_d lnovftx =	{ 0xfefa39ef, 0x40862e42 };/* chopped */
static union_ul_d lnunftx =	{ 0xd52d3052, 0xc0874910 };/* ln(minf/2) chop*/
static union_ul_d PI_RZx =	{ 0x54442d18, 0x400921fb };
static union_ul_d invln2x =	{ 0x652b82fe, 0x3ff71547 };
static union_ul_d sqrt2x =	{ 0x667f3bcd, 0x3ff6a09e };/* rounded up */
static union_ul_d sqrt2p1_hix =	{ 0x333f9de6, 0x4003504f };
static union_ul_d sqrt2p1_lox =	{ 0xf626cdd5, 0x3ca21165 };
static union_ul_ld inflx  = { 0x00000000, 0x80000000, 0x00007fff };
static union_ul_ld minslx = { 0x00000001, 0x00000000, 0x00000000 };
static union_ul_ld maxslx = { 0xffffffff, 0xffffffff, 0x00000000 };
static union_ul_ld minnlx = { 0x00000000, 0x80000000, 0x00000001 };
static union_ul_ld maxlx  = { 0xffffffff, 0xffffffff, 0x00007ffe };
static union_ul_ld NaNlx  = { 0x00000000, 0xc0000000, 0x00007fff };
static union_ul_ld sNaNlx = { 0x00000001, 0x80000000, 0x00007fff };

#endif	/* i386 */

#define ln2 		ln2x.d
#define ln2hi 		ln2hix.d
#define ln2lo 		ln2lox.d
#define NaN 		NaNx.d
#define Inf 		Infx.d
#define two52 		two52x.d
#define twom52 		twom52x.d
#define fmax 		fmaxx.d
#define fmin 		fminx.d
#define sNaN 		sNaNx.d
#define fmaxs 		fmaxsx.d
#define fminn 		fminnx.d
#define lnovft 		lnovftx.d
#define lnunft 		lnunftx.d
#define PI_RZ 		PI_RZx.d
#define sqrt2 		sqrt2x.d
#define invln2 		invln2x.d
#define sqrt2p1_hi 	sqrt2p1_hix.d
#define sqrt2p1_lo 	sqrt2p1_lox.d

#define infl		inflx.ld
#define minsl		minslx.ld
#define maxsl		maxslx.ld
#define minnl		minnlx.ld
#define maxl		maxlx.ld
#define NaNl		NaNlx.ld
#define sNaNl		sNaNlx.ld

#endif /* _LIBM_CONSTANTS */

#endif /* ifdef _ASM */
