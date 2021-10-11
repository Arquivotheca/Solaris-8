%{
/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided this notice is preserved and that due credit is given
 * to Sun Microsystems, Inc.  The name of Sun Microsystems, Inc. may
 * not be used to endorse or promote products derived from this
 * software without specific prior written permission.  This software
 * is provided ``as is'' without express or implied warranty.
 */
#pragma ident	"@(#)lex.l	1.3	96/09/10 SMI"



#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "gram.h"
#include "rdb.h"
%}

ws		[ \t]+
nl		\n
symbol		[_a-zA-Z][_a-zA-Z0-9]*
varstring	\$[_a-zA-Z][_a-zA-Z0-9]*	/* $<name> */
hexnumber	0[xX][0-9a-zA-Z]+
decnumber	[0-9]+
qstring		\"[^\"\n]*[\"\n]

%%
\#[^\n]*		;	/* igore comments */
\\\n			;	/* perform line continuation... */
{ws}			;	/* ignore whitespace */
{hexnumber}		{yylval.addr = hexstr_to_num(yytext); return (NUMBER);}
{decnumber}		{yylval.addr = atoi(yytext); return (NUMBER);}
\+			{return (PLUS);}
^{ws}*break		{return (BREAK);}
^{ws}*cont		{return (CONT);}
^{ws}*echo		{return (ECHO_OUT);}
^{ws}*event		{return (EVENT);}
^{ws}*delete		{return (DELETE);}
^{ws}*dis		{return (DIS);}
^{ws}*getmaps		{return (GETMAPS);}
^{ws}*help		{return (HELP);}
^{ws}*linkmaps		{return (LINKMAPS);}
^{ws}*maps		{return (MAPS);}
^{ws}*objpad		{return (OBJPAD);}
^{ws}*pltskip		{return (PLTSKIP);}
^{ws}*print		{return (PRINT);}
^{ws}*step		{return (STEP);}
^{ws}*value		{return (VALUE);}
^{ws}*where		{return (WHERE);}
{symbol}		{yylval.str = strdup(yytext); return (SYMBOL);}
{varstring}		{
				yylval.str = strdup(yytext + 1);
				return (VARSTRING);
			}
{qstring}		{
				yylval.str = strdup(yytext + 1);
				if (yylval.str[yyleng - 2] == '"')
					yylval.str[yyleng - 2] = '\0';
				return (QSTRING);
			}
{nl}			{return (NEWLINE);}
%%
