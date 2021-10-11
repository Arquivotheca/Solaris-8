/*	Copyright (c) 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)reject.c	6.8	98/01/28 SMI"

#include <stdio.h>

#ifdef EUC
#include <euc.h>
#include <widec.h>
#include <limits.h>
#endif


#ifndef JLSLEX
#pragma weak yyinput
#pragma weak yyleng
#pragma weak yytext
#pragma weak yyunput

#define	CHR    char
#define	YYTEXT yytext
#define	YYLENG yyleng
#define	YYINPUT yyinput
#define	YYUNPUT yyunput
#define	YYOUTPUT yyoutput
#define	YYREJECT yyreject
#endif

#ifdef WOPTION
#pragma weak yyinput
#pragma weak yyleng
#pragma weak yytext
#pragma weak yyunput

#define	CHR    wchar_t
#define	YYTEXT yytext
#define	YYLENG yyleng
#define	YYINPUT yyinput
#define	YYUNPUT yyunput
#define	YYOUTPUT yyoutput
#define	YYREJECT yyreject_w
#endif

#ifdef EOPTION
#pragma weak yyleng
#pragma weak yytext
#pragma weak yywinput
#pragma weak yywleng
#pragma weak yywtext
#pragma weak yywunput

#define	CHR    wchar_t
#define	YYTEXT yywtext
#define	YYLENG yywleng
#define	YYINPUT yywinput
#define	YYUNPUT yywunput
#define	YYOUTPUT yywoutput
#define	YYREJECT yyreject_e
extern unsigned char yytext[];
extern int yyleng;
#endif

#pragma weak yyback
#if defined(__cplusplus) || defined(__STDC__)
extern int	yyback(int *, int);
extern int	YYINPUT(void);
extern void	YYUNPUT(int);
#ifdef EUC
	static int	yyracc(int);
#else
	extern int	yyracc(int);
#endif
#ifdef EOPTION
	extern size_t	wcstombs(char *, const wchar_t *, size_t);
#endif
#endif

#pragma weak yyout
extern FILE *yyout, *yyin;

#pragma weak yyfnd
#pragma weak yyprevious
extern int yyprevious, *yyfnd;

#pragma weak yyextra
extern char yyextra[];

extern int YYLENG;
extern CHR YYTEXT[];

#pragma weak yylsp
#pragma weak yylstate
#pragma weak yyolsp
extern struct {int *yyaa, *yybb; int *yystops; } *yylstate[], **yylsp, **yyolsp;
#if defined(__cplusplus) || defined(__STDC__)
int
YYREJECT(void)
#else
YYREJECT()
#endif
{
	for (; yylsp < yyolsp; yylsp++)
		YYTEXT[YYLENG++] = YYINPUT();
	if (*yyfnd > 0)
		return (yyracc(*yyfnd++));
	while (yylsp-- > yylstate) {
		YYUNPUT(YYTEXT[YYLENG-1]);
		YYTEXT[--YYLENG] = 0;
		if (*yylsp != 0 && (yyfnd = (*yylsp)->yystops) && *yyfnd > 0)
			return (yyracc(*yyfnd++));
	}
#ifdef EOPTION
	yyleng = wcstombs((char *)yytext, YYTEXT, YYLENG*MB_LEN_MAX);
#endif
	if (YYTEXT[0] == 0)
		return (0);
	YYLENG = 0;
#ifdef EOPTION
	yyleng = 0;
#endif
	return (-1);
}

#if defined(__cplusplus) || defined(__STDC__)
int
yyracc(int m)
#else
yyracc(m)
#endif
{
	yyolsp = yylsp;
	if (yyextra[m]) {
		while (yyback((*yylsp)->yystops, -m) != 1 && yylsp > yylstate) {
			yylsp--;
			YYUNPUT(YYTEXT[--YYLENG]);
		}
	}
	yyprevious = YYTEXT[YYLENG-1];
	YYTEXT[YYLENG] = 0;
#ifdef EOPTION
	yyleng = wcstombs((char *)yytext, YYTEXT, YYLENG*MB_LEN_MAX);
#endif
	return (m);
}
