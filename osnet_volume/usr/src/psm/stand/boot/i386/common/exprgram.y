/*
 * Copyright (c) 1992 Sun Microsystems, Inc.  All Rights Reserved.
 */

/* #ident "@(#)exprgram.y	1.6	96/04/08 SMI" */

/* yacc grammar for parsing expressions */

%{

#include <sys/types.h>
#include <sys/salib.h>
#include <sys/bsh.h>

extern int yylex();
extern long strtol(char *str, char **ptr, int base);

#define	YYEXPAND(x)	0
extern int expr_value;
extern int findstr();

%}

%start value

%union
{
	char *strval;
	int intval;
}

%token  <strval> STRING 1
%token  OROR            2
%token  ANDAND          3
%token  OR              4
%token  EXOR            5
%token  AND             6
%token  EQ              7
%token  NE              8
%token  LT              9
%token  GT              10
%token  LE              11
%token  GE              12
%token  SHL             13
%token  SHR             14
%token  PLUS            15
%token  MINUS           16
%token  UMINUS          17
%token  MUL             18
%token  DIV             19
%token  MOD             20
%token  COMPL           21
%token  NOT             22
%token  LPAREN          23
%token  RPAREN          24
%token  COMMA           25
%token  STRFIND         26
%token  STRCMP          27
%token  STRNCMP         28
%token  STREQ           29
%token  STRNEQ          30

%left   OROR
%left   ANDAND
%left   OR
%left   EXOR
%left   AND
%left   EQ NE
%left   LT GT LE GE
%left   SHR SHL
%left   PLUS MINUS
%left   MUL DIV MOD
%left   UMINUS COMPL NOT

%type   <intval> value expr

%%

value   : expr
	{
		expr_value = $1;
	}
	;

expr    : LPAREN expr RPAREN
	{
		$$ = $2;
	}
	| expr OROR expr
	{
		$$ = ($1 || $3);
	}
	| expr ANDAND expr
	{
		$$ = ($1 && $3);
	}
	| expr OR expr
	{
		$$ = ($1 | $3);
	}
	| expr EXOR expr
	{
		$$ = ($1 ^ $3);
	}
	| expr AND expr
	{
		$$ = ($1 & $3);
	}
	| expr EQ expr
	{
		$$ = ($1 == $3);
	}
	| expr NE expr
	{
		$$ = ($1 != $3);
	}
	| expr LT expr
	{
		$$ = ($1 < $3);
	}
	| expr GT expr
	{
		$$ = ($1 > $3);
	}
	| expr LE expr
	{
		$$ = ($1 <= $3);
	}
	| expr GE expr
	{
		$$ = ($1 >= $3);
	}
	| expr PLUS expr
	{
		$$ = ($1 + $3);
	}
	| expr MINUS expr
	{
		$$ = ($1 - $3);
	}
	| expr SHR expr
	{
		$$ = ($1 >> $3);
	}
	| expr SHL expr
	{
		$$ = ($1 << $3);
	}
	| expr MUL expr
	{
		$$ = ($1 * $3);
	}
	| expr DIV expr
	{
		$$ = ($1 / $3);
	}
	| expr MOD expr
	{
		$$ = ($1 % $3);
	}
	| MINUS expr  %prec UMINUS
	{
		$$ = (- $2);
	}
	| COMPL expr
	{
		$$ = (~ $2);
	}
	| NOT expr
	{
		$$ = (! $2);
	}
	| STRING
	{
		$$ = strtol($1, (char **)NULL, 0);
	}
	| STRFIND LPAREN STRING COMMA expr COMMA expr RPAREN
	{
	 /* $1      $2     $3     $4   $5    $6   $7    $8 */
		$$ = findstr($3, (char *)$5, $7);
	}
	| STRCMP LPAREN STRING COMMA STRING RPAREN
	{
		$$ = strcmp($3, $5);
	}
	| STRNCMP LPAREN STRING COMMA STRING COMMA expr RPAREN
	{
		$$ = strncmp($3, $5, $7);
	}
	| STREQ LPAREN STRING COMMA STRING RPAREN
	{
		$$ = (strcmp($3, $5) == 0);
	}
	| STRNEQ LPAREN STRING COMMA STRING COMMA expr RPAREN
	{
		$$ = (strncmp($3, $5, $7) == 0);
	}
	;

%%

void
yyerror(s)
	char *s;
{
	printf("boot: %s\n", s);
}
