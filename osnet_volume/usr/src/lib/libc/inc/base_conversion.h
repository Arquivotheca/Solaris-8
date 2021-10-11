/*
 * Copyright (c) 1990-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)base_conversion.h	1.18	96/12/05 SMI"

#include <errno.h>
#include <floatingpoint.h>

#ifdef DEBUG
#include <stdio.h>
#include <assert.h>
#endif

/* Sun floating-point PRIVATE include file.  */

/* PRIVATE MACROS	 */

#ifdef DEBUG
#define	PRIVATE
#else
#define	PRIVATE static
#endif

/* PRIVATE CONSTANTS	 */

#define	SINGLE_BIAS	  127
#define	DOUBLE_BIAS	 1023
#define	EXTENDED_BIAS	16383
#define	QUAD_BIAS	16383

#define	SINGLE_MAXE	  97	/* Maximum decimal exponent we need to */
				/* consider. */
#define	DOUBLE_MAXE	 771	/* Maximum decimal exponent we need to */
				/* consider. */
#define	EXTENDED_MAXE  12330	/* Maximum decimal exponent we need to */
				/* consider. */
#define	QUAD_MAXE  12330	/* Maximum decimal exponent we need to */
				/* consider. */

#define	UNPACKED_SIZE	5	/* Size of unpacked significand.  */

/* PRIVATE TYPES */

typedef struct {		/* Unpack floating-point internal format. */
				/* Value is 0.s0s1..sn * 2**(1+exponent) */
	int		sign;
	enum fp_class_type fpclass;
	int		exponent;	/* Unbiased exponent. */
	unsigned	significand[UNPACKED_SIZE];	/* Last word is round */
							/* and sticky. */
}
	unpacked;

#ifdef i386
typedef struct {		/* Most significant word formats. */
	unsigned	significand:23;
	unsigned	exponent:8;
	unsigned	sign:1;
}
	single_msw;

typedef struct {
	unsigned	significand:20;
	unsigned	exponent:11;
	unsigned	sign:1;
}
	double_msw;

typedef struct {
	unsigned	exponent:15;
	unsigned	sign:1;
	unsigned	unused:16;
}
	extended_msw;

typedef struct {
	unsigned	significand:16;
	unsigned	exponent:15;
	unsigned	sign:1;
}
	quadruple_msw;

typedef struct {		/* Floating-point formats in detail. */
	single_msw	msw;
}
	single_formatted;

typedef struct {
	unsigned	significand2;
	double_msw	msw;
}
	double_formatted;

typedef struct {
	unsigned	significand2;
	unsigned	significand;
	extended_msw	msw;
}
	extended_formatted;

typedef struct {
	unsigned	significand4;
	unsigned	significand3;
	unsigned	significand2;
	quadruple_msw	msw;
}
	quadruple_formatted;
#else
typedef struct {		/* Most significant word formats. */
	unsigned	sign:1;
	unsigned	exponent:8;
	unsigned	significand:23;
}
	single_msw;

typedef struct {
	unsigned	sign:1;
	unsigned	exponent:11;
	unsigned	significand:20;
}
	double_msw;

typedef struct {
	unsigned	sign:1;
	unsigned	exponent:15;
	unsigned	unused:16;
}
	extended_msw;

typedef struct {
	unsigned	sign:1;
	unsigned	exponent:15;
	unsigned	significand:16;
}
	quadruple_msw;

typedef struct {		/* Floating-point formats in detail. */
	single_msw	msw;
}
	single_formatted;

typedef struct {
	double_msw	msw;
	unsigned	significand2;
}
	double_formatted;

typedef struct {
	extended_msw	msw;
	unsigned	significand;
	unsigned	significand2;
}
	extended_formatted;

typedef struct {
	quadruple_msw   msw;
	unsigned	significand2;
	unsigned	significand3;
	unsigned	significand4;
}
	quadruple_formatted;
#endif

typedef union {			/* Floating-point formats equivalenced. */
	single_formatted f;
	single		x;
}
	single_equivalence;

typedef union {
	double_formatted f;
	double		x;
}
	double_equivalence;

typedef union {
	extended_formatted f;
	extended	x;
}
	extended_equivalence;

typedef union {
	quadruple_formatted f;
	quadruple	x;
}
	quadruple_equivalence;

/* PRIVATE GLOBAL VARIABLES */

#ifdef	_REENTRANT

extern int * _thr_get_exceptions(void);
extern int * _thr_get_direction(void);
extern int * _thr_get_precision(void);
extern int * _thr_get_inf_read(void);
extern int * _thr_get_inf_written(void);
extern int * _thr_get_nan_read(void);
extern int * _thr_get_nan_written(void);


#define	_fp_current_exceptions	(*(int *)_thr_get_exceptions())
#define	_fp_current_direction	(*(int *)_thr_get_direction())
#define	_fp_current_precision	(*(int *)_thr_get_precision())
#define	__inf_read		(*(int *)_thr_get_inf_read())
#define	__inf_written		(*(int *)_thr_get_inf_written())
#define	__nan_read		(*(int *)_thr_get_nan_read())
#define	__nan_written		(*(int *)_thr_get_nan_written())

#else

/* Current floating-point exceptions. */
extern fp_exception_field_type _fp_current_exceptions;

extern enum fp_direction_type
	_fp_current_direction;	/* Current rounding direction. */

extern enum fp_precision_type
	_fp_current_precision;	/* Current rounding precision. */

extern int __inf_read, __inf_written, __nan_read, __nan_written;
	/*
	 * Flags to record reading or writing ASCII inf/nan representations
	 * for ieee_retrospective.
	*/

#endif _REENTRANT

extern double __base_conversion_write_only_double;
	/*
	 * Areas for writing garbage.  Reference them to fool dead code
	 * elimination.
	*/

/* PRIVATE FUNCTIONS */

extern void
__fp_normalize(unpacked *);
		/* unpacked operand and result */

extern void
__fp_leftshift(unpacked *, unsigned);
/*
 * unpacked       *pu; unsigned        n;
 */

extern void
__fp_set_exception(enum fp_exception_type);
/* enum fp_exception_type ex; */	/* exception to be set in curexcep */

extern void
__base_conversion_set_exception(fp_exception_field_type);
		/* set exception to cause hardware traps */

/*
 * Default size for _big_float - suitable for single and double precision.
 */

#define	_BIG_FLOAT_SIZE	(DECIMAL_STRING_LENGTH/2)
#define	_BIG_FLOAT_DIGIT unsigned short	/* big_float significand type */

typedef struct {		/* Variable-precision floating-point type */
				/* used for intermediate results.	 */
	unsigned short  bsize;
				/*
				 * Maximum allowable logical length of
				 * significand.
				*/
	unsigned short  blength; /* Logical length of significand. */
	short int	bexponent;
					/*
					 * Exponent to be attached to least
					 * significant word of significand.
					 * exponent >= 0 implies all integer,
					 * with decimal point to right of
					 * least significant word of
					 * significand, and is equivalent to
					 * number of omitted trailing zeros
					 * of significand. -length < exponent
					 * < 0  implies decimal point within
					 * significand. exponent = -length
					 * implies decimal point to left of
					 * most significand word. exponent <
					 * -length implies decimal point to
					 * left of most significant word with
					 * -length-exponent leading zeros.
					*/
	/*
	 * NOTE: bexponent represents a power of 2 or 10, even though big
	 * digits are powers of 2**16 or 10**4.
	 */
	_BIG_FLOAT_DIGIT bsignificand[_BIG_FLOAT_SIZE];
	/*
	 * Significand of digits in base 10**4 or 2**16. significand[0] is
	 * least significant, significand[length-1] is most significant.
	 */
}	_big_float;

#define	BIG_FLOAT_TIMES_NOMEM	(_big_float *)0
#define	BIG_FLOAT_TIMES_TOOBIG	(_big_float *)1

/* Internal functions defined in base conversion support routines. */

extern void	__gconvert(double, int, int, char *);
extern void	__multiply_base_ten(_big_float *, _BIG_FLOAT_DIGIT);
extern void	__multiply_base_two_carry(void);
extern void	__multiply_base_ten_by_two(_big_float *, short unsigned);
extern void	__multiply_base_two(_big_float *, _BIG_FLOAT_DIGIT,
		unsigned int);
extern void	__multiply_base_two_by_two(void);
extern void	__carry_propagate_two(unsigned int,  _BIG_FLOAT_DIGIT *);
extern void	__carry_propagate_ten(unsigned int,  _BIG_FLOAT_DIGIT *);
extern void	__multiply_base_two_vector(unsigned short, _BIG_FLOAT_DIGIT *,
		unsigned short *, _BIG_FLOAT_DIGIT []);
extern void	__multiply_base_ten_vector(unsigned short, _BIG_FLOAT_DIGIT *,
		unsigned short *, _BIG_FLOAT_DIGIT []);
extern void	__four_digits_quick(unsigned short, char *);
extern void	__big_binary_to_big_decimal(_big_float *, _big_float *);
extern void	__left_shift_base_ten(_big_float *, unsigned short);
extern void	__left_shift_base_two(_big_float *, unsigned short);
extern void	__right_shift_base_two(_big_float *, unsigned short,
		_BIG_FLOAT_DIGIT *);
extern void	__free_big_float(_big_float *);
extern void	__base_conversion_abort(int, char *);
extern void	__display_big_float(_big_float *, unsigned);
extern void	__integerstring_to_big_decimal(char [], unsigned, unsigned,
		unsigned *, _big_float *);
extern void	__fractionstring_to_big_decimal(char [], unsigned, unsigned,
		_big_float *);
extern void	__big_decimal_to_big_binary(_big_float *, _big_float *);
extern void	__double_to_decimal(double *px, decimal_mode *pm,
			decimal_record *pd, fp_exception_field_type *ps);
extern void	__k_double_to_decimal(double dd, decimal_mode *pm,
		decimal_record *pd, fp_exception_field_type *ps);

extern void	__quadruple_to_decimal(quadruple *px, decimal_mode *pm,
			decimal_record *pd, fp_exception_field_type *ps);
extern void	__fp_rightshift(unpacked *, int);
extern void	__fp_leftshift(unpacked *, unsigned);
extern void	__fp_normalize(unpacked *);
extern void	_split_shorten(_big_float *);
extern void	__pack_single(unpacked *, single *);
extern void	__pack_double(unpacked *, double *);
extern void	__pack_extended(unpacked *, extended *);
extern void	__pack_quadruple(unpacked *, quadruple *);
extern void	__unpack_single(unpacked *, single *);
extern void	__unpack_double(unpacked *, double *);
extern void 	_unpacked_to_decimal_two(_big_float *, _big_float *,
		decimal_mode *, decimal_record *, fp_exception_field_type *);
extern enum fp_class_type	__class_single(single *);
extern enum fp_class_type	__class_double(double *);
extern enum fp_class_type	__class_extended(extended *);
extern enum fp_class_type	__class_quadruple(quadruple *);
extern void	__infnanstring(enum fp_class_type cl, int ndigits, char *buf);

extern	void	__big_binary_to_unpacked(_big_float *, unpacked *);
/*
 * Whether to use floating-point or integer arithmetic varies among
 * implementations; floating point seems to have a slight edge on
 * Sunrise+FPU2 but not on Sirius+FPA or Sun-386+80387
*/

#if 0	/* Tried this for SPARC, causes FP exceptions in wrong places */
#define	QUOREM10000(A, B) /* A = A/10000, B=A%10000, for int A > 0 */\
			/* and short unsigned B */ \
	{ int ___iq; int ___ir; \
	___ir = (A); \
	___iq = ___ir * 1.0e-4; \
	___ir -= 1.0e4 * ___iq; \
	if (___ir < 0) { ___ir += 10000; ___iq -= 1; } else \
	if (___ir >= 10000) { ___ir -= 10000; ___iq += 1; } \
	B = ___ir; \
	A = ___iq; \
	}
#define	LONGQUOREM10000(A, B) /* A = A/10000, B=A%10000, */\
				/* for unsigned A and short unsigned B */ \
	{ int ___iq; int ___ir; \
	___ir = (A); \
	___iq = ___ir * 1.0e-4; \
	___ir -= 1.0e4 * ___iq; \
	if (___ir < 0) { ___ir += 10000; ___iq -= 1; } else \
	if (___ir >= 10000) { ___ir -= 10000; ___iq += 1; } \
	B = ___ir; \
	A = ___iq; \
	}
#else
#define	QUOREM10000(A, B) /* A = A/10000, B=A%10000, for int A > 0 */\
			/* and short unsigned B */ \
	A = __quorem10000((unsigned int)(A), &(B))
#define	LONGQUOREM10000(A, B) /* A = A/10000, B = A%10000, */\
				/* for unsigned A and short unsigned B */ \
	A = __longquorem10000((unsigned int)(A), &(B))
#endif

/*
 * Fundamental utilities that multiply or add two shorts into a unsigned int,
 * sometimes add an unsigned int carry,
 * compute quotient and remainder in underlying base, and return
 * quo<<16 | rem as  a unsigned int.
*/

extern unsigned short __quorem(unsigned int, unsigned short, unsigned short *);
extern unsigned short __quorem10000(unsigned int, unsigned short *);
extern unsigned int __longquorem10000(unsigned int, unsigned short *);

extern unsigned int __umac(_BIG_FLOAT_DIGIT, _BIG_FLOAT_DIGIT, unsigned int);
/* p = x * y + c ; return p */
/* extern unsigned int __prodc_b65536(); */	/* p = x * y + c ; return p */
#define	__prodc_b65536(x, y, c) (__umac((x), (y), (c)))
extern unsigned int __prodc_b10000(_BIG_FLOAT_DIGIT, _BIG_FLOAT_DIGIT,
		unsigned int);



/* p = x * y + c ; return (p/10000 << */
extern unsigned int __prod_10000_b65536(_BIG_FLOAT_DIGIT, unsigned int);
/* p = x * 10000 + c ; return */
						/* p */
/* extern unsigned int _rshift_b65536(); */ /* p = x << n + c<<16 ; return p */
#define	_rshift_b65536(x, n, c) ((((unsigned int) (x)) << \
				(16-(n))) + ((c)<<16))
/* extern unsigned int __lshift_b65536(); */	/* p = x << n + c ; return p */
#define	__lshift_b65536(x, n, c) ((((unsigned int) (x)) << (n)) + (c))
extern unsigned int __lshift_b10000(_BIG_FLOAT_DIGIT, unsigned short,
		unsigned int);
/* p = x << n + c; return (p/10000 */
/* << 16 | p%10000) */
/* extern unsigned int __carry_in_b65536(); */	/* p = x + c ; return p */
#define	__carry_in_b65536(x, c) ((x) + (c))
extern unsigned int __carry_in_b10000(_BIG_FLOAT_DIGIT, unsigned int);
						/* (p/10000 << 16 | p%10000) */
/* extern unsigned int __carry_out_b65536(); */	/* p = c ; return p */
#define	__carry_out_b65536(c) (c)
#define	__carry_out_b10000(coz, coc) /* p = c; return (p/10000 << 16 | */\
					/* p%10000) */ \
{ \
	unsigned int   cop; \
	unsigned short cor; \
 \
	cop = __quorem10000((unsigned int) (coc), &cor); \
	coz = (unsigned int) ((cop  << 16) | cor); \
}

extern void	__mul_65536short(unsigned int, _BIG_FLOAT_DIGIT *,
		unsigned short *);
extern unsigned int ___mul_65536_n(unsigned int,  _BIG_FLOAT_DIGIT *, int);

/*
 * Header file for revised "fast" base conversion based upon table look-up
 * methods.
*/

extern void
__big_float_times_power(
/* function to multiply a big_float times a positive power of two or ten. */
			_big_float * pbf,
						/*
						 * Operand x, to be replaced
						 * by the product x * mult **
						 * n.
						*/
	int		mult,
				/*
				 * if mult is two, x is base 10**4; if mult
				 * is ten, x is base 2**16
				*/
	int		n,
	int		precision,
					/*
					 * Number of bits of precision
					 * ultimately required (mult=10) or
					 * number of digits of precision
					 * ultimately required (mult=2).
					 * Extra bits are allowed internally
					 * to permit correct rounding.
					*/
	_big_float    **pnewbf
				/*
				 * Return result *pnewbf is set to: pbf f
				 * uneventful: *pbf holds the product ;
				 * BIG_FLOAT_TIMES_TOOBIG	if n is
				 * bigger than the tables permit ;
				 * BIG_FLOAT_TIMES_NOMEM	if
				 * pbf->blength was insufficient to hold the
				 * product, and malloc failed to produce a
				 * new block; &newbf			if
				 * pbf->blength was insufficient to hold the
				 * product, and a new _big_float was
				 * allocated by malloc. newbf holds the
				 * product. It's the caller's responsibility
				 * to free this space when no longer needed.
				*/
);

/*	TABLE-DRIVEN FLOATING-POINT BASE CONVERSION	*/

/*
 * The run-time structure consists of two large tables of powers - either
 * powers of 5 in base 2**16 or powers of 2 in base 10**4.
 *
 * __tbl_10_small_digits	contains
 *	5**0,
 *	5**1, ...
 *	5**__TBL_10_SMALL_SIZE-1
 * __tbl_10_big_digits		contains
 *	5**0,
 *	5**__TBL_10_SMALL_SIZE, ...
 *	5**__TBL_10_SMALL_SIZE*(__TBL_10_BIG_SIZE-1)
 * __tbl_10_huge_digits		contains
 *	5**0,
 *	5**__TBL_10_SMALL_SIZE*__TBL_10_BIG_SIZE, ...
 *	5**__TBL_10_SMALL_SIZE*__TBL_10_BIG_SIZE*(__TBL_10_HUGE_SIZE-1)
 *
 * so that any power of 5 from 5**0 to
 *	5**__TBL_10_SMALL_SIZE*__TBL_10_BIG_SIZE*__TBL_10_HUGE_SIZE
 * can be represented as a product of at most three table entries.
 *
 * Similarly any power of 2 from 2**0 to
 *	2**__TBL_2_SMALL_SIZE*__TBL_2_BIG_SIZE*__TBL_2_HUGE_SIZE
 * can be represented as a product of at most three table entries.
 *
 * Since the powers vary greatly in
 * size, the tables are condensed to exclude leading and trailing zeros.
 *
 * The powers are stored consecutively in the tables, with the start index table
 * at the end.
 *
 * Entry i in table x is stored in
 *	x_digits[x_start[i]] (least significant)
 * through
 *	x_digits[x_start[i+1]-1] (most significant)
 *
*/

#define	__TBL_10_SMALL_SIZE	64
#define	__TBL_10_BIG_SIZE	16
#define	__TBL_10_HUGE_SIZE	5
extern const unsigned short __tbl_10_small_digits[],
				__tbl_10_small_start[], __tbl_10_big_digits[],
				__tbl_10_big_start[], __tbl_10_huge_digits[],
				__tbl_10_huge_start[];

#define	__TBL_2_SMALL_SIZE	176
#define	__TBL_2_BIG_SIZE	16
#define	__TBL_2_HUGE_SIZE	6
extern const unsigned short __tbl_2_small_digits[],
				__tbl_2_small_start[], __tbl_2_big_digits[],
				__tbl_2_big_start[], __tbl_2_huge_digits[],
				__tbl_2_huge_start[];

/*	Other tables used to hasten floating-point base conversion.	*/

typedef char	__four_digits_quick_string[4];
extern const __four_digits_quick_string __four_digits_quick_table[];
	/* Table to translate short unsigned < 10000 to four ASCII digits. */


/*
 * The following functions are used in floating-point base conversion
 * using floating-point arithmetic.
 */

#define	__TBL_TENS_EXACT 22	/* The max n such that 10**n fits exactly in */
				/* IEEE double. */
#define	__TBL_TENS_MAX 49	/* The max n such that 10**n is in the table. */
#define	__TBL_TENS_SIZE (__TBL_TENS_MAX+1)
/* Dimension of table of powers of ten. */
#define	__TBL_BASELG_SIZE 128	/* Size of table of log10(1.f) */

enum __fbe_type {		/* Types of errors detected in floating-point */
				/* base conversion. */
	__fbe_none,		/* No errors detected. */
	__fbe_one,		/* One correct rounding error occurred. */
	__fbe_many		/* Grosser errors render result useless. */
};

extern const double   __tbl_tens[__TBL_TENS_SIZE];
/* Table of positive powers of ten */
extern const double   __tbl_ntens[__TBL_TENS_SIZE];
/* Table of negative powers of ten */

extern const short unsigned __tbl_baselg[__TBL_BASELG_SIZE];
/* Table of log10(1.f) for use in base conversion. */

typedef struct { int status, mode; } __ieee_flags_type;
				/* Struct for storing IEEE mode/status. */

extern void __get_ieee_flags(__ieee_flags_type *);
		/* Returns IEEE mode/status and sets up */
				/* standard environment for base conversion. */
extern void __set_ieee_flags(__ieee_flags_type *);
		/* Restores previous IEEE mode/status. */

extern double   __abs_ulp(double);
		/* Determines absolute value of 1 ulp of arg. */

extern double   __add_set(double, double, enum __fbe_type *);
/* Adds two doubles, returns result and */
/* exceptions. */
extern double   __mul_set(double, double, enum __fbe_type *);
/* Multiplies two doubles, returns result and */
				/* exceptions. */
extern double   __div_set(double, double, enum __fbe_type *);
/* Divides two doubles, returns result and */
				/* exceptions. */
extern double   __arint(double);
extern double   __arint_set_n(double, int, enum __fbe_type *);
/*
 * Converts double with rounding errors to integral value, returns result and
 * exceptions.
*/

extern double   __digits_to_double(char *, int, enum __fbe_type *);
/*
 * Converts n decimal ascii digits into double.  Up to 15 digits are always
 * exactly representable.
*/

extern void	__double_to_digits(double, char *, int *);
/* Converts an integral double x to n<=16 digits. */

extern double	__dabs(double);	/* Private version of fabs */

/* prototypes in <sunmath.h> */

extern double min_normal(void);
extern double signaling_nan(int);

extern enum fp_direction_type _QgetRD(void);
extern void __k_gconvert(int, decimal_record *, int, char *);
