/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)fpasciisr.c	1.11	98/07/28 SMI"

#include <sys/types.h>
#include "adb.h"
#include "fpascii.h"

extern char *ecvt(), *strchr();

/*
 * Floating-point register print routines.
 *
 * Note: Allow Fusion kadb to use these routines, since it needs to
 * display the floating point registers (the kernel uses them to
 * implement bcopy()).
 */

#if	!defined(KADB) || defined(sun4u)

/*
 * Prints single precision number d.
 */
void
singletos(f, s)
	float f;	/* may be widened due to C rules */
	char *s;
{
   int decpt, sign;
   char *ecp;

	ecp = ecvt( f, 16, &decpt, &sign );
	if( sign ) *s++ = '-';
	*s++ = *ecp++;		/* copy first digit */
	*s++ = '.';		/* decimal point */
	strcpy( s, ecp );	/* copy the rest of the number */
	s = strchr( s, 0 );	/* ptr to end of s */

	sprintf( s, "E%03d", decpt );
}


/*
 * Prints double precision number d.
 */
void
doubletos(d, s)
	double d;
	char *s;
{
   int decpt, sign;
   char *ecp;

	ecp = ecvt( d, 16, &decpt, &sign );
	if( sign ) *s++ = '-';
	*s++ = *ecp++;		/* copy first digit */
	*s++ = '.';		/* decimal point */
	strcpy( s, ecp );	/* copy the rest of the number */
	s = strchr( s, 0 );	/* ptr to end of s */

	sprintf( s, "E%03d", decpt );
}


/*
 * Print 16 fpu registers in ascending order. The 'firstreg'
 * parameter is the starting # of the sequence. This is used by
 * the $x/$X and (for SPARC-V9) $y/$Y commands.
 *
 * Note that in V9, although the odd numbered "upper" (%f32-64) FP regs
 * are not directly addressable, the information is diplayed anyway to
 * maintain consistency with $x/$X and allow users, who may wish to pair
 * 32-bit values and store them in the FP registers (to avoid going to
 * memory), to be able to view the registers as individual 32-bit
 * quantities.
 */
void
printfpuregs(firstreg)
	int firstreg;
{
	int i, r, pval, val = 0;
	union {
		struct {
			int i1;
			int i2;
		} ip;	/* "int_pair" */
		float	float_overlay;
		double	dbl_overlay;
	} u_ipfd;	/* union of int-pair, float, and double */
	extern int v9flag;

	printf("%-5s %8X\n", regnames[Reg_FSR], readreg(Reg_FSR)) ;
	if (v9flag) {
		prxregset_t *xp = (prxregset_t *)&xregs;
		printf("fprs  %8X\n", xp->pr_un.pr_v8p.pr_fprs);
	}

	for (i = 0; i <= 15; ++i, pval = val) {
		r = firstreg + i;
		val = readreg(r) ;
		printf("%-5s %8X", regnames[r], val) ;
		u_ipfd.ip.i1 = val;
		printf("   %f", u_ipfd.float_overlay);

		if (i & 1) {
			u_ipfd.ip.i1 = pval;
			u_ipfd.ip.i2 = val;
			printf("   %F", u_ipfd.dbl_overlay);
		}
		printf("\n");
	}
}
#endif	/* !defined(KADB) || defined(sun4u) */
