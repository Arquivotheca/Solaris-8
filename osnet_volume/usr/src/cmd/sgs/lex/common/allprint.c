/*	Copyright (c) 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)allprint.c	6.7	97/12/08 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/euc.h>
#include <ctype.h>
#include <widec.h>
#include <wctype.h>

#pragma weak yyout

#ifndef JLSLEX
#define	CHR    char
#endif

#ifdef WOPTION
#define	CHR	wchar_t
#define	sprint	sprint_w
#endif

#ifdef EOPTION
#define	CHR	wchar_t
#endif

void
allprint(c)
CHR c;
{
	extern FILE *yyout;
	switch (c) {
	case '\n':
		fprintf(yyout, "\\n");
		break;
	case '\t':
		fprintf(yyout, "\\t");
		break;
	case '\b':
		fprintf(yyout, "\\b");
		break;
	case ' ':
		fprintf(yyout, "\\_");
		break;
	default:
		if (!iswprint(c))
		    fprintf(yyout, "\\x%-2x", c);
		else
		    putwc(c, yyout);
		break;
	}
}

void
sprint(s)
CHR *s;
{
	while (*s)
		allprint(*s++);
}
