/*
 * Copyright (c) 1988-1996 by Sun Microsystems, Inc.
 * Copyright (c) 1988 by Nihon Sun Microsystems K.K.
 * All rights reserved.
 */

#pragma ident	"@(#)euclen.c	1.8	96/11/15 SMI"   /* Nihon Sun Micro JLE */

#include	"synonyms.h"
#include	<euc.h>
#include	<ctype.h>

/*
 * euccol(s) returns the screen column width of the EUC char.
 */
int
euccol(const unsigned char *s)
{

	if (ISASCII(*s))
		return (1);
	else
		switch (*s) {
		case SS2:
			return (scrw2);
		case SS3:
			return (scrw3);
		default: /* code set 1 */
			return (scrw1);
		}
}

/*
 * euclen(s,n) returns the code width of the  EUC char.
 * May also be implemented as a macro.
 */
int
euclen(const unsigned char *s)
{

	if (ISASCII(*s))
		return (1);
	else
		switch (*s) {
		case SS2:
			return (eucw2 + 1); /* include SS2 */
		case SS3:
			return (eucw3 + 1); /* include SS3 */
		default: /* code set 1 */
			return (eucw1);
		}
}

/* this function will return the number of display column for a */
/* given euc string.						*/
int
eucscol(const unsigned char *s)

{
	int	col = 0;

	while (*s) { /* end if euc char is a NULL character */
		if (ISASCII(*s)) {
			col += 1;
			s++;
		}
		else
			switch (*s) {
			case SS2:
				col += scrw2;
				s += (eucw2 +1);
				break;
			case SS3:
				col += scrw3;
				s += (eucw3 +1);
				break;
			default:	/* code set 1 */
				col += scrw1;
				s += eucw1;
				break;
			}

	}
	return (col);
}
