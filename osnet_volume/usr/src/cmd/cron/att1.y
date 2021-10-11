/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

%{
#ident	"@(#)att1.y	1.6	95/03/07 SMI"	/* SVr4.0 1.4	*/
%}
%{

#include "stdio.h"
#include "ctype.h"
#include "time.h"

extern int yylex(void);
extern void yyerror();
extern void atabort(char *);
int yyparse(void);
extern	int	gmtflag;
extern	int	mday[];
extern	struct	tm *tp, at, rt;
%}
%token	TIME
%token	NOW
%token	NOON
%token	MIDNIGHT
%token	MINUTE
%token	HOUR
%token	DAY
%token	WEEK
%token	MONTH
%token	YEAR
%token	UNIT
%token	SUFF
%token	ZONE
%token	AM
%token	PM
%token	ZULU
%token	NEXT
%token	NUMB
%token	COLON
%token	COMMA
%token	PLUS
%token	UNKNOWN
%right	NUMB
%%

args
	: time ampm timezone date incr {
		if (at.tm_min >= 60 || at.tm_hour >= 24)
			atabort("bad time");
		if (at.tm_mon >= 12 || at.tm_mday > mday[at.tm_mon])
			atabort("bad date");
		if (at.tm_year >= 1900)
			at.tm_year -= 1900;
		if (at.tm_year < 70 || at.tm_year >= 139)
			atabort("bad year");
	}
	| time ampm timezone date incr UNKNOWN {
		yyerror();
	}
	;

time
	: hour {
		at.tm_hour = $1;
	}
	| hour COLON number {
		at.tm_hour = $1;
		at.tm_min = $3;
		$3 = $1;
	}
	| hour minute {
		at.tm_hour = $1;
		at.tm_min = $2;
		$2 = $1;
	}
	| TIME {
		switch ($1) {
		case NOON:
			at.tm_hour = 12;
			break;
		case MIDNIGHT:
			at.tm_hour = 0;
			break;
		case NOW:
			at.tm_hour = tp->tm_hour;
			at.tm_min = tp->tm_min;
			at.tm_sec = tp->tm_sec;
			break;
		}
	}
	;

ampm
	: /*empty*/
	{
	}
	| SUFF {
		switch ($1) {
		case PM:
			if (at.tm_hour < 1 || at.tm_hour > 12)
				atabort("bad hour");
				at.tm_hour %= 12;
				at.tm_hour += 12;
				break;
		case AM:
			if (at.tm_hour < 1 || at.tm_hour > 12)
				atabort("bad hour");
			at.tm_hour %= 12;
			break;
		}
	}
	;

timezone
	: /*empty*/
	{
	}
	| ZONE {
		if (at.tm_hour == 24 && at.tm_min != 0)
			atabort("bad time");
		at.tm_hour %= 24;
		gmtflag = 1;
	}
	;

date
	: /*empty*/ {
		at.tm_mday = tp->tm_mday;
		at.tm_mon = tp->tm_mon;
		at.tm_year = tp->tm_year;
		if ((at.tm_hour < tp->tm_hour)
			|| ((at.tm_hour==tp->tm_hour)&&(at.tm_min<tp->tm_min)))
			rt.tm_mday++;
	}
	| MONTH number {
		at.tm_mon = $1;
		at.tm_mday = $2;
		at.tm_year = tp->tm_year;
		if (at.tm_mon < tp->tm_mon)
			at.tm_year++;
	}
	| MONTH number COMMA number {
		at.tm_mon = $1;
		at.tm_mday = $2;
		at.tm_year = $4;
	}
	| DAY {
		at.tm_mon = tp->tm_mon;
		at.tm_mday = tp->tm_mday;
		at.tm_year = tp->tm_year;
		if ($1 < 7) {
			rt.tm_mday = $1 - tp->tm_wday;
			if (rt.tm_mday < 0)
				rt.tm_mday += 7;
		} else if ($1 == 8)
			rt.tm_mday += 1;
	}
	;

incr
	: /*empty*/
	| NEXT UNIT	{ addincr:
		switch ($2) {
		case MINUTE:
			rt.tm_min += $1;
			break;
		case HOUR:
			rt.tm_hour += $1;
			break;
		case DAY:
			rt.tm_mday += $1;
			break;
		case WEEK:
			rt.tm_mday += $1 * 7;
			break;
		case MONTH:
			rt.tm_mon += $1;
			break;
		case YEAR:
			rt.tm_year += $1;
			break;
		}
	}
	| PLUS opt_number UNIT { goto addincr; }
	;

hour
	: NUMB		{ $$ = $1; }
	| NUMB NUMB	{ $$ = 10 * $1 + $2; }
	;
minute
	: NUMB NUMB	{ $$ = 10 * $1 + $2; }
	;
number
	: NUMB		{ $$ = $1; }
	| number NUMB	{ $$ = 10 * $1 + $2; }
	;
opt_number
	: /* empty */	{ $$ = 1; }
	| number	{ $$ = $1; }
	;

%%
