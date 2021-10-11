/*	Copyright (c) 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)yyless.c	6.10	98/02/08 SMI"

#include <sys/euc.h>
#include <stdlib.h>
#include <widec.h>
#include <limits.h>
#include <inttypes.h>
#include <unistd.h>

#pragma weak yyprevious

#ifndef JLSLEX
#pragma weak yyinput
#pragma weak yyleng
#pragma weak yyunput
#pragma weak yytext

#define	CHR    char
#define	YYTEXT yytext
#define	YYLENG yyleng
#define	YYINPUT yyinput
#define	YYUNPUT yyunput
#define	YYOUTPUT yyoutput
#endif

#ifdef WOPTION
#pragma weak yyinput
#pragma weak yyleng
#pragma weak yyunput
#pragma weak yytext

#define	CHR    wchar_t
#define	YYTEXT yytext
#define	YYLENG yyleng
#define	YYINPUT yyinput
#define	YYUNPUT yyunput
#define	YYOUTPUT yyoutput
#endif

#ifdef EOPTION
#pragma weak yyleng
#pragma weak yytext
#pragma weak yywinput
#pragma weak yywleng
#pragma weak yywunput
#pragma weak yywtext

#define	CHR    wchar_t
#define	YYTEXT yywtext
#define	YYLENG yywleng
#define	YYINPUT yywinput
#define	YYUNPUT yywunput
#define	YYOUTPUT yywoutput
#endif

#if defined(__STDC__)
    extern void YYUNPUT(int);
#endif

#if defined(__cplusplus) || defined(__STDC__)
/* XCU4: type of yyless() changes to int */
int
yyless(int x)
#else
yyless(x)
int x;
#endif
{
	extern CHR YYTEXT[];
	register CHR *lastch, *ptr;
	extern int YYLENG;
	extern int yyprevious;
#ifdef EOPTION
	extern char yytext[];
	extern int yyleng;
#endif
	lastch = YYTEXT+YYLENG;
	if (x >= 0 && x <= YYLENG)
		ptr = x + YYTEXT;
	else {
#ifdef	_LP64
		static int seen = 0;

		if (!seen) {
			(void) write(2,
				    "warning: yyless pointer arg truncated\n",
				    39);
			seen = 1;
		}
#endif	/* _LP64 */
	/*
	 * The cast on the next line papers over an unconscionable nonportable
	 * glitch to allow the caller to hand the function a pointer instead of
	 * an integer and hope that it gets figured out properly.  But it's
	 * that way on all systems.
	 */
		ptr = (CHR *) x;
	}
	while (lastch > ptr)
		YYUNPUT(*--lastch);
	*lastch = 0;
	if (ptr > YYTEXT)
		yyprevious = *--lastch;
	YYLENG = ptr-YYTEXT;
#ifdef EOPTION
	yyleng = wcstombs(yytext, YYTEXT, YYLENG*MB_LEN_MAX);
#endif
	return (0);
}
