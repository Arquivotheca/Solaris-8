/*	Copyright (c) 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)libmai.c	6.6	97/12/08 SMI"

#include <locale.h>

#pragma weak yyparse
extern int yyparse(void);

main()
{
	setlocale(LC_ALL, "");
	yyparse();
	return (0);
}
