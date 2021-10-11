/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)octet.c	1.5	99/08/18 SMI"

/* LINTLIBRARY */

#if !defined(_BOOT) && !defined(_KERNEL)
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#endif	/* _BOOT && _KERNEL */
#include <sys/types.h>
#include <sys/errno.h>

#if	defined(_BOOT) || defined(_KERNEL)
#define	NULL		0
#define	isdigit(c)	((c) >= '0' && c <= '9')
#endif	/* _BOOT || _KERNEL */

/*
 * Converts an octet string into an ASCII string. The string returned is
 * NULL-terminated, and the length recorded in blen does *not* include the
 * null terminator (in other words, octet_to_ascii() returns the length a'la
 * strlen()).
 *
 * Returns 0 for Success, errno otherwise.
 */
int
octet_to_ascii(uint8_t *nump, int nlen, char *bufp, int *blen)
{
	int		i;
	char		*bp;
	uint8_t		*np;
	static char	ascii_conv[] = {'0', '1', '2', '3', '4', '5', '6',
	    '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

	if (nump == NULL || bufp == NULL || blen == NULL)
		return (EINVAL);

	if ((nlen * 2) >= *blen) {
		*blen = 0;
		return (E2BIG);
	}
	for (i = 0, bp = bufp, np = nump; i < nlen; i++) {
		*bp++ = ascii_conv[(np[i] >> 4) & 0x0f];
		*bp++ = ascii_conv[np[i] & 0x0f];
	}
	*bp = '\0';
	*blen = i * 2;
	return (0);
}

/*
 * Converts an ASCII string into an octet string.
 *
 * Returns 0 for success, errno otherwise.
 */
int
ascii_to_octet(char *asp, int alen, uint8_t *bufp, int *blen)
{
	int	i, j, k;
	char	*tp;
	uint8_t	*u_tp;

	if (asp == NULL || bufp == NULL || blen == NULL)
		return (EINVAL);

	if (alen > (*blen * 2))
		return (E2BIG);

	k = ((alen % 2) == 0) ? alen / 2 : (alen / 2) + 1;
	for (tp = asp, u_tp = bufp, i = 0; i < k; i++, u_tp++) {
		/* one nibble at a time */
		for (*u_tp = 0, j = 0; j < 2; j++, tp++) {
			if (isdigit(*tp))
				*u_tp |= *tp - '0';
			else
				*u_tp |= (*tp & ~0x20) + 10 - 'A';
			if ((j % 2) == 0)
				*u_tp <<= 4;
		}
	}
	*blen = k;
	return (0);
}
