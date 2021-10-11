/*
 * Copyright (c) 1991-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)softmath64.c	1.2	99/10/04 SMI"

#if defined(lint)

extern void main(void);

#else	/* lint */

/*
 * These routines are to support the compiler run-time only, and
 * should NOT be called directly from C!
 */

#define	W 64		/* bits in a word */
#define	B 4		/* number base of division (must be a power of 2) */
#define	N 2		/* log2(B) */
#define	WB (W/N)	/* base B digits in a word */
#define	Q dividend	/* re-use the dividend as the partial quotient */
#define	Big_value (1ull<<(W-N-1))  /* (B ^ WB-1)/2 */

/*
 * An 'inline' routine that does a 'ta 2' to simulate the effects of
 * a hardware divide-by-zero.
 */

extern long long __raise_divide_by_zero(void);

long long
__div64(long long dividend, long long divisor)
{
	long long		R;	/* partial remainder */
	unsigned long long 	V;	/* multiple of the divisor */
	int iter, sign = 0;

	if (divisor == 0)
		return (__raise_divide_by_zero());
	if (dividend < 0) {
		dividend = -dividend;
		sign = 1;
	}
	if (divisor < 0) {
		divisor = -divisor;
		sign ^= 1;
	}

	/*
	 * -(-2^63) == -2^63, so compare unsigned long long, so that
	 * -2^63 as divisor, or -2^63 as dividend, works.
	 */
	if (dividend < (unsigned long long)divisor)
		return ((long long)0);

	if (!((unsigned)(dividend >> 32) | (unsigned)(divisor >> 32))) {
		Q = (unsigned long long)((unsigned)dividend/(unsigned)divisor);
		goto ret;
	}

	R = dividend;
	V = divisor;
	iter = 0;
	if (R >= Big_value) {
		int SC;

		for (; V < Big_value; iter++)
			V <<= N;
		for (SC = 0; V < R; SC++) {
			if ((long long)V < 0)
				break;
			V <<= 1;
		}
		R -= V;
		Q = 1;
		while (--SC >= 0) {
			Q <<= 1;
			V >>= 1;
			if (R >= 0) {
				R -= V;
				Q += 1;
			} else {
				R += V;
				Q -= 1;
			}
		}
	} else {
		Q = 0;
		do {
			V <<= N;
			iter++;
		} while (V <= R);
	}

	while (--iter >= 0) {
		Q <<= N;
		/* N-deep, B-wide decision tree */
		V >>= 1;
		if (R >= 0) {
			R -= V;
			V >>= 1;
			if (R >= 0) {
				R -= V;
				Q += 3;
			} else {
				R += V;
				Q += 1;
			}
		} else {
			R += V;
			V >>= 1;
			if (R >= 0) {
				R -= V;
				Q -= 1;
			} else {
				R += V;
				Q -= 3;
			}
		}
	}
	if (R < 0)
		Q -= 1;
ret:
	return (sign ? -Q : Q);
}

long long
__rem64(long long dividend, long long divisor)
{
	long long		R;	/* partial remainder */
	unsigned long long	V;	/* multiple of the divisor */
	int iter, sign = 0;

	if (divisor == 0)
		return (__raise_divide_by_zero());
	if (dividend < 0) {
		dividend = -dividend;
		sign = 1;
	}
	if (divisor < 0)
		divisor = -divisor;

	/*
	 * -(-2^63) == -2^63, so compare unsigned long long so that
	 * x % -2^63 works.
	 */
	if ((unsigned long long)divisor == 1)
		return ((long long)0);

	/* Compare unsigned long long, so that -2^63 % x works. */
	if ((unsigned long long)dividend < divisor) {
		R = dividend;
		goto ret;
	}
	if (!((unsigned)(dividend >> 32) | (unsigned)(divisor >> 32))) {
		R = (unsigned long long)((unsigned)dividend%(unsigned)divisor);
		goto ret;
	}

	R = dividend;
	V = divisor;
	iter = 0;
	if (R >= Big_value) {
		int SC;

		for (; V < Big_value; iter++)
			V <<= N;
		for (SC = 0; V < R; SC++) {
			if ((long long)V < 0)
				break;
			V <<= 1;
		}
		R -= V;
		Q = 1;
		while (--SC >= 0) {
			Q <<= 1;
			V >>= 1;
			if (R >= 0) {
				R -= V;
				Q += 1;
			} else {
				R += V;
				Q -= 1;
			}
		}
	} else {
		Q = 0;
		do {
			V <<= N;
			iter++;
		} while (V <= R);
	}

	while (--iter >= 0) {
		Q <<= N;
		/* N-deep, B-wide decision tree */
		V >>= 1;
		if (R >= 0) {
			R -= V;
			V >>= 1;
			if (R >= 0) {
				R -= V;
				Q += 3;
			} else {
				R += V;
				Q += 1;
			}
		} else {
			R += V;
			V >>= 1;
			if (R >= 0) {
				R -= V;
				Q -= 1;
			} else {
				R += V;
				Q -= 3;
			}
		}
	}
	if (R < 0)
		R += divisor;
ret:
	return (sign ? -R : R);
}

unsigned long long
__udiv64(unsigned long long dividend, unsigned long long divisor)
{
	long long		R;	/* partial remainder */
	unsigned long long	V;	/* multiple of the divisor */
	int iter;

	if (divisor == 0)
		return (__raise_divide_by_zero());
	if (dividend < divisor)
		return ((unsigned long long)0);
	if (!((unsigned)(dividend >> 32) | (unsigned)(divisor >> 32)))
		return ((unsigned long long)
			((unsigned)dividend/(unsigned)divisor));

	R = dividend;
	V = divisor;
	iter = 0;
	if (R >= Big_value) {
		int SC;

		for (; V < Big_value; iter++)
			V <<= N;
		for (SC = 0; V < R; SC++) {
			if ((long long)V < 0)
				break;
			V <<= 1;
		}
		R -= V;
		Q = 1;
		while (--SC >= 0) {
			Q <<= 1;
			V >>= 1;
			if (R >= 0) {
				R -= V;
				Q += 1;
			} else {
				R += V;
				Q -= 1;
			}
		}
	} else {
		Q = 0;
		do {
			V <<= N;
			iter++;
		} while (V <= R);
	}

	while (--iter >= 0) {
		Q <<= N;
		/* N-deep, B-wide decision tree */
		V >>= 1;
		if (R >= 0) {
			R -= V;
			V >>= 1;
			if (R >= 0) {
				R -= V;
				Q += 3;
			} else {
				R += V;
				Q += 1;
			}
		} else {
			R += V;
			V >>= 1;
			if (R >= 0) {
				R -= V;
				Q -= 1;
			} else {
				R += V;
				Q -= 3;
			}
		}
	}
	if (R < 0)
		Q -= 1;
	return (Q);
}

unsigned long long
__urem64(unsigned long long dividend, unsigned long long divisor)
{
	long long		R;	/* parital remainder */
	unsigned long long	V;	/* multiple of the divisor */
	int iter;

	if (divisor == 0)
		return (__raise_divide_by_zero());
	else if (divisor == 1)
		return ((unsigned long long)0);
	if (dividend < divisor)
		return (dividend);
	if (!((unsigned)(dividend >> 32) | (unsigned)(divisor >> 32)))
		return ((unsigned long long)
			((unsigned)dividend%(unsigned)divisor));

	R = dividend;
	V = divisor;
	iter = 0;
	if (R >= Big_value) {
		int SC;

		for (; V < Big_value; iter++)
			V <<= N;
		for (SC = 0; V < R; SC++) {
			if ((long long)V < 0)
				break;
			V <<= 1;
		}
		R -= V;
		Q = 1;
		while (--SC >= 0) {
			Q <<= 1;
			V >>= 1;
			if (R >= 0) {
				R -= V;
				Q += 1;
			} else {
				R += V;
				Q -= 1;
			}
		}
	} else {
		Q = 0;
		do {
			V <<= N;
			iter++;
		} while (V <= R);
	}

	while (--iter >= 0) {
		Q <<= N;
		/* N-deep, B-wide decision tree */
		V >>= 1;
		if (R >= 0) {
			R -= V;
			V >>= 1;
			if (R >= 0) {
				R -= V;
				Q += 3;
			} else {
				R += V;
				Q += 1;
			}
		} else {
			R += V;
			V >>= 1;
			if (R >= 0) {
				R -= V;
				Q -= 1;
			} else {
				R += V;
				Q -= 3;
			}
		}
	}
	if (R < 0)
		R += divisor;
	return (R);
}

#endif	/* lint */
