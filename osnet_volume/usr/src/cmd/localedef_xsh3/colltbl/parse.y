/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

%{
#ident	"@(#)parse.y	1.9	94/01/23 SMI"	/* SVr4.0 1.1.1.1	*/

/* Preprocessor Statements */
#include "colltbl.h"

#define	SZ_COLLATE	256
#define	ORD_LST		1
#define	PAR_LST		2
#define	BRK_LST		3

/* External Variables */
extern char	codeset[];
extern int	curprim;
extern int	cursec;

static int	ordtype = ORD_LST;
static int	begrng = -1;
static int	cflag = 0;
static int	oflag = 0;


/* Redefined Error Message Handler */
void yyerror(text)
char *text;
{
	error(YYERR, text);
}
%}

%union {
	unsigned char	*sval;
}

/* Token Types */
%token		ELLIPSES
%token	<sval>	ID
%token		IS
%token		CODESET
%token		ORDER
%token		SUBSTITUTE
%token		SEPARATOR
%token	<sval>	STRING
%token	<sval>	SYMBOL
%token		WITH

%type	<sval>	symbol
%type	<sval>	error
%%
collation_table : statements
			{
			if (!cflag || !oflag)
		error(PRERR, "codeset or order statement not specified");
			}
		;

statements 	: statement
		| statements statement
		;

statement	: codeset_stmt
			{
			if (cflag)
			error(PRERR, "multiple codeset statements seen");
			cflag++;
			}
		| order_stmt
			{
			if (oflag)
				error(PRERR, "multiple order statements seen");
			oflag++;
		}
		| substitute_stmt
		| error
		{
		error(EXPECTED, "codeset, order or substitute statement");
		}
		;

codeset_stmt	: CODESET ID
		{
			if (strlen((char *)$2) >= 50)
				error(TOO_LONG, "file name", $2);
			if (strlen((char *)$2) == 0)
				error(NOT_DEFINED, "codeset name");
			strcpy(codeset, (char *)$2);
		}
		;

substitute_stmt	: SUBSTITUTE STRING WITH STRING
		{
			substitute($2, $4);
		}
		;

order_stmt	: ORDER IS order_list
		;

order_list	: order_element
		| order_list SEPARATOR order_element
		;

order_element	: symbol
		| lparen sub_list rparen
		{
			ordtype = ORD_LST;
			cursec = 1;
		}
		| lbrace sub_list rbrace
		{
			ordtype = ORD_LST;
		}
		| error
		{
			error(INVALID, "order element", $1);
		}
		;

lparen		: '('
		{
			ordtype = PAR_LST;
			++curprim;
			cursec = 2;
		}
		;

rparen		: ')'
		;

lbrace		: '{'
		{
			ordtype = BRK_LST;
			++curprim;
		}
		;

rbrace		: '}'
		;

sub_list	: sub_element
		| sub_list SEPARATOR sub_element
		| error
		{
			error(INVALID, "list", "inter-filed");
		}
		;

sub_element	: symbol
		;

symbol		: SYMBOL
		{
			if (strlen((char *)$1) == 1)
				begrng = (unsigned char) *$1;
			else
				begrng = -1;
			mkord((unsigned char *)$1, ordtype);
		}
		| ELLIPSES SEPARATOR SYMBOL
		{
			static unsigned char	*tarr = (unsigned char *)"?";
			int	i, n;

			if (begrng < 0 || strlen((char *)$3) != 1 ||
				((unsigned int)*$3 <= begrng))
				error(PRERR, "bad list range");
			n = (unsigned int)*$3 - begrng;
			for (i=0; i<n; i++) {
				begrng++;
				tarr[0] = (unsigned char)begrng;
				mkord(tarr, ordtype);
			}
		}
		;
