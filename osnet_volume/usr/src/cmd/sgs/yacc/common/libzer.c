/*	Copyright (c) 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)libzer.c	6.3	97/12/08 SMI"

#include <stdio.h>

#ifdef __cplusplus
void
yyerror(const char *s)
#else
yyerror(s)
char *s;
#endif
{
	fprintf(stderr, "%s\n", s);
	return (0);
}
