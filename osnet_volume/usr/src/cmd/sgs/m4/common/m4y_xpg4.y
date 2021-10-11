/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

%{
#pragma ident	"@(#)m4y_xpg4.y	1.1	95/06/16 SMI"
extern long	evalval;
#define	YYSTYPE	long
%}

%term DIGITS
%left OROR
%left ANDAND
%left '|'
%left '^'
%left '&'
%nonassoc EQ NE
%nonassoc LE GE LT GT
%left LSHIFT RSHIFT
%left '+' '-'
%left '*' '/' '%'
%right POWER
%right '!' '~' UMINUS
%%

s	: e	= { evalval = $1; }
	|	= { evalval = 0; }
	;

e	: e OROR e	= { $$ = ($1 != 0 || $3 != 0) ? 1 : 0; }
	| e ANDAND e	= { $$ = ($1 != 0 && $3 != 0) ? 1 : 0; }
	| '!' e		= { $$ = $2 == 0; }
	| '~' e		= { $$ = ~$2; }
	| e EQ e	= { $$ = $1 == $3; }
	| e NE e	= { $$ = $1 != $3; }
	| e GT e	= { $$ = $1 > $3; }
	| e GE e	= { $$ = $1 >= $3; }
	| e LT e	= { $$ = $1 < $3; }
	| e LE e	= { $$ = $1 <= $3; }
	| e LSHIFT e	= { $$ = $1 << $3; }
	| e RSHIFT e	= { $$ = $1 >> $3; }
	| e '|' e	= { $$ = ($1 | $3); }
	| e '&' e	= { $$ = ($1 & $3); }
	| e '^' e	= { $$ = ($1 ^ $3); }
	| e '+' e	= { $$ = ($1 + $3); }
	| e '-' e	= { $$ = ($1 - $3); }
	| e '*' e	= { $$ = ($1 * $3); }
	| e '/' e	= { $$ = ($1 / $3); }
	| e '%' e	= { $$ = ($1 % $3); }
	| '(' e ')'	= { $$ = ($2); }
	| e POWER e	= { for ($$ = 1; $3-- > 0; $$ *= $1); }
	| '-' e %prec UMINUS	= { $$ = $2-1; $$ = -$2; }
	| '+' e %prec UMINUS	= { $$ = $2-1; $$ = $2; }
	| DIGITS	= { $$ = evalval; }
	;

%%

#include <ctype.h>
extern char *pe;

int peek(char c, int r1, int r2);
int peek3(char c1, int rc1, char c2, int rc2, int rc3);

yylex() {
	while (*pe == ' ' || *pe == '\t' || *pe == '\n')
		pe++;
	switch (*pe) {
	case '\0':
	case '+':
	case '-':
	case '/':
	case '%':
	case '^':
	case '~':
	case '(':
	case ')':
		return (*pe++);
	case '*':
		return (peek('*', POWER, '*'));
	case '>':
		return (peek3('=', GE, '>', RSHIFT, GT));
	case '<':
		return (peek3('=', LE, '<', LSHIFT, LT));
	case '=':
		return (peek('=', EQ, EQ));
	case '|':
		return (peek('|', OROR, '|'));
	case '&':
		return (peek('&', ANDAND, '&'));
	case '!':
		return (peek('=', NE, '!'));
	default: {
		register	base;

		evalval = 0;

		if (*pe == '0') {
			if (*++pe == 'x' || *pe == 'X') {
				base = 16;
				++pe;
			} else
				base = 8;
		} else
			base = 10;

		for (;;) {
			register	c, dig;

			c = *pe;

			if (isdigit(c))
				dig = c - '0';
			else if (c >= 'a' && c <= 'f')
				dig = c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				dig = c - 'A' + 10;
			else
				break;

			evalval = evalval*base + dig;
			++pe;
		}
		return (DIGITS);
	}
	}
}

int
peek(char c, int r1, int r2)
{
	if (*++pe != c)
		return (r2);
	++pe;
	return (r1);
}

int
peek3(char c1, int rc1, char c2, int rc2, int rc3)
{
	++pe;
	if (*pe == c1) {
		++pe;
		return(rc1);
	}
	if (*pe == c2) {
		++pe;
		return(rc2);
	}
	return (rc3);
}

/* VARARGS */
void
yyerror()
{}
