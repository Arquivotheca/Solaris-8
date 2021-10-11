/*    @(#)base_conversion.h	1.2	96/06/07 SMI	*/

/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

#include <errno.h>

#include <floatingpoint.h>

#ifdef DEBUG
#include <stdio.h>
#include <assert.h>
#endif

/* Sun floating-point PRIVATE include file.  */

/* PRIVATE MACROS	 */

#ifdef DEBUG
#define PRIVATE static
#else
#define PRIVATE static
#endif

/* PRIVATE CONSTANTS	 */

#define	SINGLE_BIAS	  127
#define DOUBLE_BIAS	 1023
#define EXTENDED_BIAS	16383
#define QUAD_BIAS	16383

#define SINGLE_MAXE	  97	/* Maximum decimal exponent we need to
				 * consider. */
#define DOUBLE_MAXE	 771	/* Maximum decimal exponent we need to
				 * consider. */
#define EXTENDED_MAXE  12330	/* Maximum decimal exponent we need to
				 * consider. */
#define QUAD_MAXE  12330	/* Maximum decimal exponent we need to
				 * consider. */

#define UNPACKED_SIZE	5	/* Size of unpacked significand.  */

/* PRIVATE TYPES 	 */

typedef struct {		/* Unpack floating-point internal format. *//* V
				 * alue is 0.s0s1..sn * 2**(1+exponent) */
	int             sign;
	enum fp_class_type fpclass;
	int             exponent;	/* Unbiased exponent. */
	unsigned        significand[UNPACKED_SIZE];	/* Last word is round
							 * and sticky. */
}
                unpacked;

#ifdef i386
typedef struct {		/* Most significant word formats. */
	unsigned        significand:23;
	unsigned        exponent:8;
	unsigned        sign:1;
}
                single_msw;

typedef struct {
	unsigned        significand:20;
	unsigned        exponent:11;
	unsigned        sign:1;
}
                double_msw;

typedef struct {
	unsigned        exponent:15;
	unsigned        sign:1;
	unsigned        unused:16;
}
                extended_msw;

typedef struct {
	unsigned        significand:16;
	unsigned        exponent:15;
	unsigned        sign:1;
}
                quadruple_msw;

typedef struct {		/* Floating-point formats in detail. */
	single_msw      msw;
}
                single_formatted;

typedef struct {
	unsigned        significand2;
	double_msw      msw;
}
                double_formatted;

typedef struct {
	unsigned        significand2;
	unsigned        significand;
	extended_msw    msw;
}
                extended_formatted;

typedef struct {
	unsigned        significand4;
	unsigned        significand3;
	unsigned        significand2;
	quadruple_msw   msw;
}
                quadruple_formatted;
#else
typedef struct {		/* Most significant word formats. */
	unsigned        sign:1;
	unsigned        exponent:8;
	unsigned        significand:23;
}
                single_msw;

typedef struct {
	unsigned        sign:1;
	unsigned        exponent:11;
	unsigned        significand:20;
}
                double_msw;

typedef struct {
	unsigned        sign:1;
	unsigned        exponent:15;
	unsigned        unused:16;
}
                extended_msw;

typedef struct {
	unsigned        sign:1;
	unsigned        exponent:15;
	unsigned        significand:16;
}
                quadruple_msw;

typedef struct {		/* Floating-point formats in detail. */
	single_msw      msw;
}
                single_formatted;

typedef struct {
	double_msw      msw;
	unsigned        significand2;
}
                double_formatted;

typedef struct {
	extended_msw    msw;
	unsigned        significand;
	unsigned        significand2;
}
                extended_formatted;

typedef struct {
	quadruple_msw   msw;
	unsigned        significand2;
	unsigned        significand3;
	unsigned        significand4;
}
                quadruple_formatted;
#endif

typedef union {			/* Floating-point formats equivalenced. */
	single_formatted f;
	single          x;
}
                single_equivalence;

typedef union {
	double_formatted f;
	double          x;
}
                double_equivalence;

typedef union {
	extended_formatted f;
	extended        x;
}
                extended_equivalence;

typedef union {
	quadruple_formatted f;
	quadruple       x;
}
                quadruple_equivalence;

/* PRIVATE GLOBAL VARIABLES */

fp_exception_field_type _fp_current_exceptions;	/* Current floating-point
						 * exceptions. */

enum fp_direction_type
                _fp_current_direction;	/* Current rounding direction. */

enum fp_precision_type
                _fp_current_precision;	/* Current rounding precision. */

double __base_conversion_write_only_double;
        /* Areas for writing garbage.  Reference them to fool dead code
           elimination.
        */

int __inf_read, __inf_written, __nan_read, __nan_written;
	/* Flags to record reading or writing ASCII inf/nan representations
	   for ieee_retrospective.
	*/

/* PRIVATE FUNCTIONS */

extern void 
__fp_normalize( /* pu */ );
#ifdef notdef
unpacked        *pu ;           /* unpacked operand and result */
#endif /* notdef */

extern void 
__fp_leftshift( /* pu, n */ );
/*
 * unpacked       *pu; unsigned        n;
 */

extern void 
__fp_set_exception( /* ex */ );
#ifdef notdef
enum fp_exception_type ex ;	/* exception to be set in curexcep */
#endif /* notdef */

extern void
__base_conversion_set_exception();	/* set exception to cause hardware traps */

/*
 * Default size for _big_float - suitable for single and double precision.
 */

#define _BIG_FLOAT_SIZE	(DECIMAL_STRING_LENGTH/2)
#define _BIG_FLOAT_DIGIT short unsigned	/* big_float significand type */

typedef struct {		/* Variable-precision floating-point type
				 * used for intermediate results.	 */
	unsigned short  bsize;	/* Maximum allowable logical length of
				 * significand. */
	unsigned short  blength;/* Logical length of significand. */
	short int       bexponent;	/* Exponent to be attached to least
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
					 * -length-exponent leading zeros. */
	/*
	 * NOTE: bexponent represents a power of 2 or 10, even though big
	 * digits are powers of 2**16 or 10**4.
	 */
	_BIG_FLOAT_DIGIT bsignificand[_BIG_FLOAT_SIZE];
	/*
	 * Significand of digits in base 10**4 or 2**16. significand[0] is
	 * least significant, significand[length-1] is most significant.
	 */
}               _big_float;

#define BIG_FLOAT_TIMES_NOMEM	(_big_float *)0
#define BIG_FLOAT_TIMES_TOOBIG	(_big_float *)1

/* Internal functions defined in base conversion support routines. */

extern void	__gconvert();
extern void     __multiply_base_ten();
extern void     __multiply_base_two_carry();
extern void     __multiply_base_ten_by_two();
extern void     __multiply_base_two();
extern void     __multiply_base_two_by_two();
extern void     __carry_propagate_two();
extern void     __carry_propagate_ten();
extern void     __multiply_base_two_vector();
extern void     __multiply_base_ten_vector();
extern void     __four_digits_quick();
extern void     __big_binary_to_big_decimal();
extern void     __left_shift_base_ten();
extern void     __left_shift_base_two();
extern void     __right_shift_base_two();
extern void     __free_big_float();
extern void	__base_conversion_abort();
extern void	__display_big_float();
extern void	__integerstring_to_big_decimal();
extern void	__fractionstring_to_big_decimal();
extern void	__big_decimal_to_big_binary();
extern void     __double_to_decimal();
extern void     __quadruple_to_decimal();
extern void	__fp_rightshift();
extern void	__fp_leftshift();
extern void	__fp_normalize();
extern void	_split_shorten();
extern void	__pack_single();
extern void	__pack_double();
extern void	__pack_extended();
extern void	__pack_quadruple();
extern void	__unpack_single();
extern void	__unpack_double();
extern void 	_unpacked_to_decimal_two();
extern enum fp_class_type	__class_single();
extern enum fp_class_type	__class_double();
extern enum fp_class_type	__class_extended();
extern enum fp_class_type	__class_quadruple();
extern void	__infnanstring();	/* Utility to copy inf infinity or nan strings. */

/*	Whether to use floating-point or integer arithmetic varies among implementations;
	floating point seems to have a slight edge on Sunrise+FPU2 but not
	on Sirius+FPA or Sun-386+80387
*/

#if 0	/* Tried this for SPARC, causes FP exceptions in wrong places */
#define QUOREM10000(A,B) /* A = A/10000, B=A%10000, for int A > 0 and short unsigned B */ \
	{ register int ___iq; register int ___ir; \
	___ir = (A); \
	___iq = ___ir * 1.0e-4; \
	___ir -= 1.0e4 * ___iq; \
	if (___ir < 0) { ___ir += 10000; ___iq -= 1 ; } else \
	if (___ir >= 10000) { ___ir -= 10000 ; ___iq += 1 ; } \
	B=___ir; \
	A=___iq; \
	}
#define LONGQUOREM10000(A,B) /* A = A/10000, B=A%10000, for unsigned A and short unsigned B */ \
	{ register int ___iq; register int ___ir; \
	___ir = (A); \
	___iq = ___ir * 1.0e-4; \
	___ir -= 1.0e4 * ___iq; \
	if (___ir < 0) { ___ir += 10000; ___iq -= 1 ; } else \
	if (___ir >= 10000) { ___ir -= 10000 ; ___iq += 1 ; } \
	B=___ir; \
	A=___iq; \
	}
#else
#define QUOREM10000(A,B) /* A = A/10000, B=A%10000, for int A > 0 and short unsigned B */ \
	A = __quorem10000((long unsigned)(A), &(B))
#define LONGQUOREM10000(A,B) /* A = A/10000, B=A%10000, for unsigned A and short unsigned B */ \
	A = __longquorem10000((long unsigned)(A), &(B))
#endif

 /*
   Fundamental utilities that multiply or add two shorts into a unsigned long, 
   sometimes add an unsigned long carry, 
   compute quotient and remainder in underlying base, and return
   quo<<16 | rem as  a unsigned long.
 */

extern unsigned short __quorem();
extern unsigned short __quorem10000();
extern unsigned long __longquorem10000();

extern unsigned long __umac();		/* p = x * y + c ; return p */
#ifdef notdef
extern unsigned long __prodc_b65536();	/* p = x * y + c ; return p */
#endif /* notdef */
#define __prodc_b65536(x,y,c) (__umac((x),(y),(c)))
extern unsigned long __prodc_b10000();	/* p = x * y + c ; return (p/10000 << */
extern unsigned long __prod_10000_b65536();	/* p = x * 10000 + c ; return
						 * p */
#ifdef notdef
extern unsigned long _rshift_b65536();		/* p = x << n + c<<16 ; return p */
#endif /* notdef */
#define _rshift_b65536(x,n,c) ((((unsigned long) (x)) << (16-(n))) + ((c)<<16))
#ifdef notdef
extern unsigned long __lshift_b65536();		/* p = x << n + c ; return p */
#endif /* notdef */
#define __lshift_b65536(x,n,c) ((((unsigned long) (x)) << (n)) + (c))
extern unsigned long __lshift_b10000();	/* p = x << n + c ; return (p/10000
					 * << 16 | p%10000) */
#ifdef notdef
extern unsigned long __carry_in_b65536();	/* p = x + c ; return p */
#endif /* notdef */
#define __carry_in_b65536(x,c) ((x) + (c))
extern unsigned long __carry_in_b10000();	/* p = x + c ; return
						 * (p/10000 << 16 | p%10000) */
#ifdef notdef
extern unsigned long __carry_out_b65536();	/* p = c ; return p */
#endif /* notdef */
#define __carry_out_b65536(c) (c)
#define __carry_out_b10000(coz,coc)          /* p = c ; return (p/10000 << 16 | p%10000) */ \
{ \
        unsigned long   cop; \
        unsigned short cor; \
 \
        cop = __quorem10000((unsigned long) (coc),&cor); \
        coz = (unsigned long) ((cop  << 16) | cor); \
}

extern unsigned long ___mul_65536_n( /* carry, ps, n */ );

/*
 * Header file for revised "fast" base conversion based upon table look-up
 * methods.
 */

extern void
__big_float_times_power(
#ifdef __STDC__
/* function to multiply a big_float times a positive power of two or ten.	 */
		       _big_float * pbf,	/* Operand x, to be replaced
						 * by the product x * mult **
						 * n. */
	int             mult,	/* if mult is two, x is base 10**4; if mult
				 * is ten, x is base 2**16 */
	int             n,
	int             precision,	/* Number of bits of precision
					 * ultimately required (mult=10) or
					 * number of digits of precision
					 * ultimately required (mult=2).
					 * Extra bits are allowed internally
					 * to permit correct rounding. */
	_big_float    **pnewbf 	/* Return result *pnewbf is set to: pbf f
				 * uneventful: *pbf holds the product ;
				 * BIG_FLOAT_TIMES_TOOBIG	if n is
				 * bigger than the tables permit ;
				 * BIG_FLOAT_TIMES_NOMEM	if
				 * pbf->blength was insufficient to hold the
				 * product, and malloc failed to produce a
				 * new block ; &newbf			if
				 * pbf->blength was insufficient to hold the
				 * product, and a new _big_float was
				 * allocated by malloc. newbf holds the
				 * product. It's the caller's responsibility
				 * to free this space when no longer needed. */
#endif
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

#define __TBL_10_SMALL_SIZE	64
#define __TBL_10_BIG_SIZE	16
#define __TBL_10_HUGE_SIZE	5
extern const unsigned short __tbl_10_small_digits[], __tbl_10_small_start[], __tbl_10_big_digits[], __tbl_10_big_start[], __tbl_10_huge_digits[], __tbl_10_huge_start[] ;

#define __TBL_2_SMALL_SIZE	176
#define __TBL_2_BIG_SIZE	16
#define __TBL_2_HUGE_SIZE	6
extern const unsigned short __tbl_2_small_digits[], __tbl_2_small_start[], __tbl_2_big_digits[], __tbl_2_big_start[], __tbl_2_huge_digits[], __tbl_2_huge_start[] ;

/*	Other tables used to hasten floating-point base conversion.	*/

extern const char __four_digits_quick_table[];  /* Table to translate short unsigned < 10000 to four ASCII digits. */


/* The following functions are used in floating-point base conversion using floating-point
arithmetic.	 */

#define __TBL_TENS_EXACT 22	/* The max n such that 10**n fits exactly in
				 * IEEE double. */
#define __TBL_TENS_MAX 49	/* The max n such that 10**n is in the table. */
#define __TBL_TENS_SIZE (__TBL_TENS_MAX+1)
/* Dimension of table of powers of ten. */
#define __TBL_BASELG_SIZE 128	/* Size of table of log10(1.f) */

enum __fbe_type {		/* Types of errors detected in floating-point
				 * base conversion. */
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

typedef struct { int status, mode ; } __ieee_flags_type;
				/* Struct for storing IEEE mode/status. */

extern void __get_ieee_flags();	/* Returns IEEE mode/status and sets up standard 
				   environment for base conversion. */
extern void __set_ieee_flags(); /* Restores previous IEEE mode/status. */

extern double   __abs_ulp();	/* Determines absolute value of 1 ulp of arg. */

extern double   __add_set();	/* Adds two doubles, returns result and
				 * exceptions. */
extern double   __mul_set();	/* Multiplies two doubles, returns result and
				 * exceptions. */
extern double   __div_set();	/* Divides two doubles, returns result and
				 * exceptions. */
extern double   __arint();
extern double   __arint_set_n();
/*
 * Converts double with rounding errors to integral value, returns result and
 * exceptions.
 */

extern double   __digits_to_double();
/*
 * Converts n decimal ascii digits into double.  Up to 15 digits are always
 * exactly representable.
 */

extern void     __double_to_digits();
/* Converts an integral double x to n<=16 digits. */

extern double	__dabs();	/* Private version of fabs */

/* prototypes in <sunmath.h> */ 

extern double min_normal(void); 
extern double signaling_nan(long);
