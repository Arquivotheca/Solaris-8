/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)istext.c	1.4	93/08/19 SMI" 	/* SVr4.0 1.	*/
#include "mail.h"

/*
 * istext(line, size) - check for text characters
 */
int
istext(s, size)
register unsigned char	*s;
int 	size;
{
	register unsigned char *ep;
	register c;
	
	for (ep = s+size; --ep >= s; ) {
		c = *ep;
		if ((!isprint(c)) && (!isspace(c)) &&
		    /* Since backspace is not included in either of the */
		    /* above, must do separately                        */
		    /* Bell character is allowable control char in the text */
		    (c != 010) && (c != 007)) {
			return(FALSE);
		}
	}
	return(TRUE);
}
