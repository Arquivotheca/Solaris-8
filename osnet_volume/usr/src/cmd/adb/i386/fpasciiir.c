/*
 * Copyright (c) 1986, 1987, 1988, 1989, 1990, 1991, 1992 by
 * Sun Microsystems, Inc.  All Rights Reserved.
 */

#ident "@(#)fpasciiir.c	1.3     92/12/23 SMI"

#include <sys/reg.h>
#include "adb.h"
#include "fpascii.h"

#if !i386

extern char *ecvt(), *strchr();

/*
 * Prints single precision number d.
 */

static void
singletos(f, s)
	float f;	/* may be widened due to C rules */
	char *s;
{
	int decpt, sign;
	char *ecp;

	db_printf(5, "singletos: f=%f", f);
	ecp = ecvt(f, 16, &decpt, &sign);
	if(sign)
		*s++ = '-';
	*s++ = *ecp++;		/* copy first digit */
	*s++ = '.';		/* decimal point */
	strcpy(s, ecp);		/* copy the rest of the number */
	s = strchr(s, 0);	/* ptr to end of s */

	sprintf(s, "E%03d", decpt);
	db_printf(5, "singletos: s=%s", s);
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

	db_printf(5, "doubletos: d=%f", d);
	ecp = ecvt(d, 16, &decpt, &sign);
	if(sign)
		*s++ = '-';
	*s++ = *ecp++;		/* copy first digit */
	*s++ = '.';		/* decimal point */
	strcpy(s, ecp);		/* copy the rest of the number */
	s = strchr(s, 0);	/* ptr to end of s */

	sprintf(s, "E%03d", decpt);
	db_printf(5, "doubletos: s=%s", s);
}


/*
 * print 16 fpu registers in ascending order. The 'firstreg'
 * parameter is the starting # of the sequence.
 */

static
printfpuregs(firstreg)
	int firstreg;
{
	int i,r,pval,val;
	char fbuf[64];

	union {
	    struct { int i1, i2; } ip;	/* "int_pair" */
	    float		   float_overlay;
	    double		   dbl_overlay;
	} u_ipfd;	/* union of int-pair, float, and double */

	printf("%-5s %8X\n", regnames[REG_FSR], readreg(REG_FSR)) ;
	for (i = 0; i <= 15 ; ++i , pval = val) {
		r = firstreg + i;

		val = readreg(r) ;
		printf("%-5s %8X", regnames[r], val) ;

		u_ipfd.ip.i1 = val;
		/* singletos(u_ipfd.float_overlay, fbuf);
		printf("   %s", fbuf);
		*/
		printf("   %f", u_ipfd.float_overlay);

		if(i&1) {
			u_ipfd.ip.i1 = pval;   u_ipfd.ip.i2 = val;
			/*
			doubletos( u_ipfd.dbl_overlay, fbuf );
			printf("   %s", fbuf);
			*/
			printf("   %F", u_ipfd.dbl_overlay );
		}
		printf("\n", fbuf);
	}
}

#endif /* !i386 */

#ifndef KADB

is_fpa_avail(void)
{
	return _fp_hw;
}

#endif /* !KADB */
