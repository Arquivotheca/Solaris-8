/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_TEST_H
#define	_TEST_H

#pragma ident	"@(#)test.h	1.1	99/05/14 SMI"

typedef enum  {
	K_STAB = 1,
	K_STR,
	K_STRC,
	K_TYPE,
	K_NUM,
	K_DESC,
	K_VAL,
	K_END
} stabkey_t;

extern FILE *yyin;
extern FILE *yyout;
extern char yytext[];
extern int yyleng;
extern int yylex(void);
extern int stab_line;

#endif	/* _TEST_H */
