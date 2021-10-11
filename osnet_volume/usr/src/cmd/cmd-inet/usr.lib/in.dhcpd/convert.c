#ident	"@(#)convert.c	1.13	97/03/27 SMI"

/*
 * Copyright (c) 1993-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * This file contains generic CONVERSION routines.
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <locale.h>

/*
 * smalloc()  --  safe malloc()
 *
 * Always returns a valid pointer(if it returns at all).  The allocated
 * memory is initialized to all zeros.  If malloc() returns an error, a
 * message is printed using the syslog() function and the program aborts
 * with a status of 1.
 *
 * Must be MT SAFE - called by threads other than the main thread.
 */
char *
smalloc(u_int nbytes)
{
	char		*retvalue;
	extern void	dhcpmsg();

	if ((retvalue = (char *)calloc(nbytes, sizeof (char))) == NULL) {
		dhcpmsg(LOG_ERR, "Cannot allocate memory (%s), exiting\n",
		    strerror(errno));
		(void) exit(1);
	}
	return (retvalue);
}

/*
 * Returns a ptr to the allocated string containing the hexidecimal
 * translation of a ASCII hex string.
 */
u_char *
get_octets(u_char **src, u_char *len)
{
	register u_char		*dest;
	register u_char		*end;
	register u_int		i;
	u_char			*ret;

	end = (u_char *)strstr((char *)*src, ":");
	if (end == (u_char *)NULL)
		end = (u_char *)(*src)[strlen((char *)*src)];

	/*
	 * Only return the *len if actual size is bigger.
	 */
	if ((u_int)((u_char *)end - *src) > (u_int)*len)
		end = (u_char *)*src + (u_int)*len;

	ret = (u_char *)smalloc((u_int)((u_char *)end - (u_char *)*src));

	if ((*src)[0] == '0' && (*src)[1] == 'x' || (*src)[1] == 'X')
		(*src) += 2;

	dest = ret;
	while (*src < end) {
		if (!isxdigit((*src)[0]) || !isxdigit((*src)[1])) {
			free(ret);
			return (NULL);
		}
		*dest = 0;
		for (*dest = 0, i = 0; i < 2; i++, (*src)++) {
			if (isdigit(**src))
				*dest |= **src - '0';
			else
				*dest |= (**src & ~0x20) + 10 - 'A';
			if (i == 0)
				*dest <<= 4;
		}
		dest++;
	}
	*len = (u_char)((u_char *)dest - (u_char *)ret);
	return (ret);
}

/*
 * Supports byte, short, long, longlong. Handles signed numbers.
 *
 * Returns 0 for success, -1 for failure. If any unexpected characters
 * are found in src, then the conversion stops there. Obviously the
 * values returned are in host order.
 */
int
get_number(char **src, void *dest, int len)
{
	register u_int	base;
	register int	neg = 0;
	register char	c;

	if (len != 1 && (len % 2) != 0 || len > 8)
		return (-1);

	/* check for sign */
	if (**src == '+' || **src == '-') {
		if (**src == '-')
			neg = 1;
		(*src)++;
	}

	/*
	 * Collect number up to first illegal character.  Values are
	 * specified as for C:  0x=hex, 0=octal, other=decimal.
	 */
	base = 10;
	if (**src == '0') {
		base = 8;
		(*src)++;
	}
	if (**src == 'x' || **src == 'X') {
		base = 16,
		(*src)++;
	}

	while ((c = **src) != 0) {
		if (isdigit(c)) {
			switch (len) {
			case 1:
				*(u_char *)dest = (*(u_char *)dest) * base +
				    (c - '0');
				break;
			case 2:
				*(u_short *)dest = (*(u_short *)dest) *
				    base + (c - '0');
				break;
			case 4:
				*(u_long *)dest = (*(u_long *)dest) *
				    base + (c - '0');
				break;
			case 8:
				*(u_longlong_t *)dest =
				    (*(u_longlong_t *)dest) * base +
				    (c - '0');
				break;
			}
			(*src)++;
			continue;
		}
		if (base == 16 && isxdigit(c)) {
			switch (len) {
			case 1:
				*(u_char *)dest =
				    ((*(u_char *)dest) << 4) + ((c & ~32) +
				    10 - 'A');
				break;
			case 2:
				*(u_short *)dest =
				    ((*(u_short *)dest) << 4) + ((c & ~32) +
				    10 - 'A');
				break;
			case 4:
				*(u_long *)dest =
				    ((*(u_long *)dest) << 4) + ((c & ~32) +
				    10 - 'A');
				break;
			case 8:
				*(u_longlong_t *)dest =
				    ((*(u_longlong_t *)dest) << 4) +
				    ((c & ~32) + 10 - 'A');
				break;
			}
		    (*src)++;
		    continue;
		}
		/* set sign */
		if (neg) {
			switch (len) {
			case 1:
				*(char *)dest = -*(char *)dest;
				break;
			case 2:
				*(short *)dest = -*(short *)dest;
				break;
			case 4:
				*(long *)dest = -*(long *)dest;
				break;
			case 8:
				*(longlong_t *)dest = -*(longlong_t *)dest;
				break;
			}
		}
		break;
	}
	return (0);
}
