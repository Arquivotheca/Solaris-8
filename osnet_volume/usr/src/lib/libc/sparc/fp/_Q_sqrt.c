/*
 * Copyright (c) 1994-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)_Q_sqrt.c	1.6	97/08/26 SMI"

#include "quad.h"

static const double C[] = {
	0.0,
	0.5,
	1.0,
	68719476736.0,
	536870912.0,
	48.0,
	16.0,
	1.52587890625000000000e-05,
	2.86102294921875000000e-06,
	5.96046447753906250000e-08,
	3.72529029846191406250e-09,
	1.70530256582424044609e-13,
	7.10542735760100185871e-15,
	8.67361737988403547206e-19,
	2.16840434497100886801e-19,
	1.27054942088145050860e-21,
	1.21169035041947413311e-27,
	9.62964972193617926528e-35,
	4.70197740328915003187e-38
};

#define	zero		C[0]
#define	half		C[1]
#define	one		C[2]
#define	two36		C[3]
#define	two29		C[4]
#define	three2p4	C[5]
#define	two4		C[6]
#define	twom16		C[7]
#define	three2m20	C[8]
#define	twom24		C[9]
#define	twom28		C[10]
#define	three2m44	C[11]
#define	twom47		C[12]
#define	twom60		C[13]
#define	twom62		C[14]
#define	three2m71	C[15]
#define	three2m91	C[16]
#define	twom113		C[17]
#define	twom124		C[18]

static const unsigned
	fsr_re = 0x00000000u,
	fsr_rn = 0xc0000000u;

#ifdef __sparcv9

/*
 * _Qp_sqrt(pz, x) sets *pz = sqrt(*x).
 */
void
_Qp_sqrt(union longdouble *pz, const union longdouble *x)

#else

/*
 * _Q_sqrt(x) returns sqrt(*x).
 */
union longdouble
_Q_sqrt(const union longdouble *x)

#endif	/* __sparcv9 */

{
#ifndef __sparcv9
	union longdouble	z;
#endif
	union xdouble		u;
	double			c, d, rr, r[2], tt[3], xx[4], zz[5];
	unsigned int		xm, fsr, lx, wx[3];
	unsigned int		msw, frac2, frac3, frac4, rm;
	int			ex, ez;

	if (QUAD_ISZERO(*x)) {
		Z = *x;
		QUAD_RETURN(Z);
	}

	xm = x->l.msw;

	__quad_getfsrp(&fsr);

	/* handle nan and inf cases */
	if ((xm & 0x7fffffff) >= 0x7fff0000) {
		if ((x->l.msw & 0xffff) | x->l.frac2 | x->l.frac3 |
		    x->l.frac4) {
			Z = *x;
			Z.l.msw |= 0x8000;
			if (!(x->l.msw & 0x8000)) {
				/* snan, signal invalid */
				if (fsr & FSR_NVM) {
					__quad_fsqrtq(x, &Z);
				} else {
					fsr = (fsr & ~FSR_CEXC) | FSR_NVA |
					    FSR_NVC;
					__quad_setfsrp(&fsr);
				}
			}
			QUAD_RETURN(Z);
		}
		if (x->l.msw & 0x80000000) {
			/* sqrt(-inf), signal invalid */
			if (fsr & FSR_NVM) {
				__quad_fsqrtq(x, &Z);
			} else {
				Z.l.msw = 0x7fffffff;
				Z.l.frac2 = Z.l.frac3 = Z.l.frac4 = 0xffffffff;
				fsr = (fsr & ~FSR_CEXC) | FSR_NVA | FSR_NVC;
				__quad_setfsrp(&fsr);
			}
			QUAD_RETURN(Z);
		}
		/* sqrt(inf), return inf */
		Z = *x;
		QUAD_RETURN(Z);
	}

	/* handle negative numbers */
	if (xm & 0x80000000) {
		if (fsr & FSR_NVM) {
			__quad_fsqrtq(x, &Z);
		} else {
			Z.l.msw = 0x7fffffff;
			Z.l.frac2 = Z.l.frac3 = Z.l.frac4 = 0xffffffff;
			fsr = (fsr & ~FSR_CEXC) | FSR_NVA | FSR_NVC;
			__quad_setfsrp(&fsr);
		}
		QUAD_RETURN(Z);
	}

	/* now x is finite, positive */
	__quad_setfsrp((unsigned*)&fsr_re);

	/* get the normalized significand and exponent */
	ex = (int) (xm >> 16);
	lx = xm & 0xffff;
	if (ex) {
		lx |= 0x10000;
		wx[0] = x->l.frac2;
		wx[1] = x->l.frac3;
		wx[2] = x->l.frac4;
	} else {
		if (lx | (x->l.frac2 & 0xfffe0000)) {
			wx[0] = x->l.frac2;
			wx[1] = x->l.frac3;
			wx[2] = x->l.frac4;
			ex = 1;
		} else if (x->l.frac2 | (x->l.frac3 & 0xfffe0000)) {
			lx = x->l.frac2;
			wx[0] = x->l.frac3;
			wx[1] = x->l.frac4;
			wx[2] = 0;
			ex = -31;
		} else if (x->l.frac3 | (x->l.frac4 & 0xfffe0000)) {
			lx = x->l.frac3;
			wx[0] = x->l.frac4;
			wx[1] = wx[2] = 0;
			ex = -63;
		} else {
			lx = x->l.frac4;
			wx[0] = wx[1] = wx[2] = 0;
			ex = -95;
		}
		while ((lx & 0x10000) == 0) {
			lx = (lx << 1) | (wx[0] >> 31);
			wx[0] = (wx[0] << 1) | (wx[1] >> 31);
			wx[1] = (wx[1] << 1) | (wx[2] >> 31);
			wx[2] <<= 1;
			ex--;
		}
	}
	ez = ex - 0x3fff;
	if (ez & 1) {
		/* make exponent even */
		lx = (lx << 1) | (wx[0] >> 31);
		wx[0] = (wx[0] << 1) | (wx[1] >> 31);
		wx[1] = (wx[1] << 1) | (wx[2] >> 31);
		wx[2] <<= 1;
		ez--;
	}

	/* extract the significands into doubles */
	c = twom16;
	xx[0] = (double) ((int) lx) * c;

	c *= twom24;
	xx[0] += (double) ((int) (wx[0] >> 8)) * c;

	c *= twom24;
	xx[1] = (double) ((int) (((wx[0] << 16) | (wx[1] >> 16)) &
	    0xffffff)) * c;

	c *= twom24;
	xx[2] = (double) ((int) (((wx[1] << 8) | (wx[2] >> 24)) &
	    0xffffff)) * c;

	c *= twom24;
	xx[3] = (double) ((int) (wx[2] & 0xffffff)) * c;

	/* approximate the divisor for the Newton iteration */
	c = xx[0] + xx[1];
	c = __quad_dp_sqrt(&c);
	rr = half / c;

	/* compute the first five "digits" of the square root */
	zz[0] = (c + two29) - two29;
	tt[0] = zz[0] + zz[0];
	r[0] = (xx[0] - zz[0] * zz[0]) + xx[1];

	zz[1] = (rr * (r[0] + xx[2]) + three2p4) - three2p4;
	tt[1] = zz[1] + zz[1];
	r[0] -= tt[0] * zz[1];
	r[1] = xx[2] - zz[1] * zz[1];
	c = (r[1] + three2m20) - three2m20;
	r[0] += c;
	r[1] = (r[1] - c) + xx[3];

	zz[2] = (rr * (r[0] + r[1]) + three2m20) - three2m20;
	tt[2] = zz[2] + zz[2];
	r[0] -= tt[0] * zz[2];
	r[1] -= tt[1] * zz[2];
	c = (r[1] + three2m44) - three2m44;
	r[0] += c;
	r[1] = (r[1] - c) - zz[2] * zz[2];

	zz[3] = (rr * (r[0] + r[1]) + three2m44) - three2m44;
	r[0] = ((r[0] - tt[0] * zz[3]) + r[1]) - tt[1] * zz[3];
	r[1] = -tt[2] * zz[3];
	c = (r[1] + three2m91) - three2m91;
	r[0] += c;
	r[1] = (r[1] - c) - zz[3] * zz[3];

	zz[4] = (rr * (r[0] + r[1]) + three2m71) - three2m71;

	/* reduce to three doubles, making sure zz[1] is positive */
	zz[0] += zz[1] - twom47;
	zz[1] = twom47 + zz[2] + zz[3];
	zz[2] = zz[4];

	/* if the third term might lie on a rounding boundary, perturb it */
	if (zz[2] == (twom62 + zz[2]) - twom62) {
		/* here we just need to get the sign of the remainder */
		c = (((((r[0] - tt[0] * zz[4]) - tt[1] * zz[4]) + r[1])
		    - tt[2] * zz[4]) - (zz[3] + zz[3]) * zz[4]) - zz[4] * zz[4];
		if (c < zero)
			zz[2] -= twom124;
		else if (c > zero)
			zz[2] += twom124;
	}

	/*
	 * propagate carries/borrows, using round-to-negative-infinity mode
	 * to make all terms nonnegative (note that we can't encounter a
	 * borrow so large that the roundoff is unrepresentable because
	 * we took care to make zz[1] positive above)
	 */
	__quad_setfsrp(&fsr_rn);
	c = zz[1] + zz[2];
	zz[2] += (zz[1] - c);
	zz[1] = c;
	c = zz[0] + zz[1];
	zz[1] += (zz[0] - c);
	zz[0] = c;

	/* adjust exponent and strip off integer bit */
	ez = (ez >> 1) + 0x3fff;
	zz[0] -= one;

	/* the first 48 bits of fraction come from zz[0] */
	u.d = d = two36 + zz[0];
	msw = u.l.lo;
	zz[0] -= (d - two36);

	u.d = d = two4 + zz[0];
	frac2 = u.l.lo;
	zz[0] -= (d - two4);

	/* the next 32 come from zz[0] and zz[1] */
	u.d = d = twom28 + (zz[0] + zz[1]);
	frac3 = u.l.lo;
	zz[0] -= (d - twom28);

	/* condense the remaining fraction; errors here won't matter */
	c = zz[0] + zz[1];
	zz[1] = ((zz[0] - c) + zz[1]) + zz[2];
	zz[0] = c;

	/* get the last word of fraction */
	u.d = d = twom60 + (zz[0] + zz[1]);
	frac4 = u.l.lo;
	zz[0] -= (d - twom60);

	/* keep track of what's left for rounding; note that the error */
	/* in computing c will be non-negative due to rounding mode */
	c = zz[0] + zz[1];

	/* get the rounding mode */
	rm = fsr >> 30;

	/* round and raise exceptions */
	fsr &= ~FSR_CEXC;
	if (c != zero) {
		fsr |= FSR_NXC;

		/* decide whether to round the fraction up */
		if (rm == FSR_RP || (rm == FSR_RN && (c > twom113 ||
		    (c == twom113 && ((frac4 & 1) || (c - zz[0] != zz[1])))))) {
			/* round up and renormalize if necessary */
			if (++frac4 == 0)
				if (++frac3 == 0)
					if (++frac2 == 0)
						if (++msw == 0x10000) {
							msw = 0;
							ez++;
						}
		}
	}

	/* stow the result */
	Z.l.msw = (ez << 16) | msw;
	Z.l.frac2 = frac2;
	Z.l.frac3 = frac3;
	Z.l.frac4 = frac4;

	if ((fsr & FSR_CEXC) & (fsr >> 23)) {
		__quad_setfsrp(&fsr);
		__quad_fsqrtq(x, &Z);
	} else {
		fsr |= (fsr & 0x1f) << 5;
		__quad_setfsrp(&fsr);
	}
	QUAD_RETURN(Z);
}
