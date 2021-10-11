/*	Copyright (c) 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)yywrap.c	6.2	93/06/07 SMI"

#if defined(__cplusplus) || defined(__STDC__)
int yywrap(void)
#else
yywrap()
#endif
{
	return(1);
}
