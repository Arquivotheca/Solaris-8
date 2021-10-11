/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_LEX_H
#define	_MDB_LEX_H

#pragma ident	"@(#)mdb_lex.h	1.1	99/08/11 SMI"

#include <mdb/mdb_argvec.h>
#include <stdio.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _MDB

extern void mdb_lex_debug(int);
extern void mdb_lex_reset(void);

extern void yyerror(const char *, ...);
extern void yyperror(const char *, ...);
extern void yydiscard(void);

extern int yyparse(void);
extern int yywrap(void);

/*
 * The lex and yacc debugging code as generated uses printf and fprintf
 * for debugging output.  We redefine these to refer to our yyprintf
 * and yyfprintf routines, which are wrappers around mdb_iob_vprintf.
 */

#define	printf	(void) yyprintf
#define	fprintf	(void) yyfprintf

extern int yyprintf(const char *, ...);
extern int yyfprintf(FILE *, const char *, ...);

extern mdb_argvec_t yyargv;
extern int yylineno;

#endif /* _MDB */

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_LEX_H */
