#if !defined(lint) && defined(SCCSIDS)
static char     sccsid[] = "@(#)__gconvert.c 1.2 92/10/16 SMI";
#endif

/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

/*
 * __k_gconvert  - Floating output conversion to minimal length string
 */

#include "synonyms.h"
#include "base_conversion.h"
#ifndef PRE41
#include <locale.h>
#endif

void
__k_gconvert(ndigits, pd, trailing, buf)
	int             ndigits;
	decimal_record *pd;
	char           *buf;
{
	char           *p;
	int             i;
#ifdef PRE41
	char            decpt = '.';
#else
	char            decpt = *(localeconv()->decimal_point);
#endif

	p = buf;
	if (pd->sign)
		*(p++) = '-';
	switch (pd->fpclass) {
	case fp_zero:
		*(p++) = '0';
		if (trailing != 0) {
			*(p++) = decpt;
			for (i = 0; i < ndigits - 1; i++)
				*(p++) = '0';
		}
		*p++ = 0;
		break;
	case fp_subnormal:
	case fp_normal:
		if ((pd->exponent > 0) || (pd->exponent < -(ndigits + 3))) {	/* E format. */
			char            estring[4];
			int             n;

			i = 0;
			*(p++) = pd->ds[0];
			*(p++) = decpt;
			for (i = 1; pd->ds[i] != 0;)
				*(p++) = pd->ds[i++];
			if (trailing == 0) {	/* Remove trailing zeros and
						 * . */
				p--;
				while (*p == '0')
					p--;
				if (*p != decpt)
					p++;
			}
			*(p++) = 'e';
			n = pd->exponent + i - 1;
			if (n >= 0)
				*(p++) = '+';
			else {
				*(p++) = '-';
				n = -n;
			}
			__four_digits_quick((short unsigned) n, estring);
			for (i = 0; estring[i] == '0'; i++);	/* Find end of zeros. */
			if (i > 2)
				i = 2;	/* Guarantee two zeros. */
			for (; i <= 3;)
				*(p++) = estring[i++];	/* Copy exp digits. */
		} else {	/* F format. */
			if (pd->exponent >= (1 - ndigits)) {	/* x.xxx */
				for (i = 0; i < (ndigits + pd->exponent);)
					*(p++) = pd->ds[i++];
				*(p++) = decpt;
				if (pd->ds[i] != 0) {	/* More follows point. */
					for (; i < ndigits;)
						*(p++) = pd->ds[i++];
				}
			} else {/* 0.00xxxx */
				*(p++) = '0';
				*(p++) = decpt;
				for (i = 0; i < -(pd->exponent + ndigits); i++)
					*(p++) = '0';
				for (i = 0; pd->ds[i] != 0;)
					*(p++) = pd->ds[i++];
			}
			if (trailing == 0) {	/* Remove trailing zeros and
						 * point. */
				p--;
				while (*p == '0')
					p--;
				if (*p != decpt)
					p++;
			}
		}
		*(p++) = 0;
		break;
	default:
		__infnanstring(pd->fpclass, ndigits, p);
		break;
	}
}
