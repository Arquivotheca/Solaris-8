/*	Copyright (c) 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)libmain.c	6.3	97/12/08 SMI"

#include "stdio.h"

extern void exit();

#pragma weak yylex
extern int  yylex();

main()
{
	yylex();
	exit(0);

	/*NOTREACHED*/
	return (0);
}
