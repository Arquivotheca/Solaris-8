%{
/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_lex.l	1.1	99/08/11 SMI"

#include <sys/types.h>
#include <sys/isa_defs.h>

#include <strings.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <mdb/mdb_types.h>
#include <mdb/mdb_debug.h>
#include <mdb/mdb_nv.h>
#include <mdb/mdb_lex.h>
#include <mdb/mdb_frame.h>
#include <mdb/mdb_string.h>
#include <mdb/mdb_stdlib.h>
#include <mdb/mdb_err.h>
#include <mdb/mdb.h>

#include "mdb_grammar.h"

/*
 * We need to undefine lex's input, unput, and output macros so that references
 * to these call the functions we provide at the end of this source file,
 * instead of the default versions based on libc's stdio.
 */
#ifdef input
#undef input
#endif

#ifdef unput
#undef unput
#endif

#ifdef output
#undef output
#endif

static int input(void);
static void unput(int);
static void output(int);

static void string_unquote(char *);

extern int yydebug;

mdb_argvec_t yyargv;
static int oldstate;
%}

%o	9000
%a	5000

%s	S_SHELLCMD
%s	S_INITIAL
%s	S_FMTLIST
%s	S_ARGLIST
%s	S_EXPR

RGX_CMD_CHAR	[?%@A-Z\^_`a-z]
RGX_SYMBOL	[a-zA-Z_.][0-9a-zA-Z_.`]*
RGX_SIMPLE_CHAR	[^ \t\n;!|"'\$]
RGX_CHR_SEQ	([^'\n]|\\[^'\n]|\\')*
RGX_STR_SEQ	([^"\\\n]|\\[^"\n]|\\\")*
RGX_COMMENT	"//".*\n

%%

<S_INITIAL>{RGX_COMMENT}	|
<S_FMTLIST>{RGX_COMMENT}	|
<S_ARGLIST>{RGX_COMMENT}	{
		/*
		 * Comments are legal in these three states -- if we see one
		 * eat the line and return the newline character.
		 */
		BEGIN(S_INITIAL);	
		return ('\n');
	}

<S_INITIAL>"=="	|
<S_EXPR>"=="	return (MDB_TOK_EQUAL); /* Equality operator */

<S_INITIAL>"!="	|
<S_EXPR>"!="	return (MDB_TOK_NOTEQUAL); /* Equality operator */

<S_INITIAL>"!"	|
<S_FMTLIST>"!"	|
<S_ARGLIST>"!"	{
		/*
		 * Shell escapes are legal in all of these states -- switch to
		 * the shell command state and return the ! character.
		 */
		BEGIN(S_SHELLCMD);
		return (yytext[0]);
	}

<S_FMTLIST>"|"	|
<S_ARGLIST>"|"	{
		/*
		 * Pipelines can appear in any of these states -- switch to
		 * the initial state and return the | character.
		 */
		BEGIN(S_INITIAL);
		return (yytext[0]);
	}

<S_SHELLCMD>[^;\n]+	{
		/*
		 * Once in the shell-command state, we return all remaining
		 * characters up to a newline or ';' delimiter as a single
		 * string which will be passed to $SHELL -c.
		 */
		yylval.l_string = strdup(yytext);
		BEGIN(S_INITIAL);
		return (MDB_TOK_STRING);
	}

<S_INITIAL>"::"{RGX_SYMBOL}	{
		/*
		 * Verb ::command-name -- lookup the correspond dcmd and
		 * switch to the argument list state.
		 */
		if ((yylval.l_dcmd = mdb_dcmd_lookup(yytext + 2)) == NULL)
			yyperror("invalid command '%s'", yytext);

		BEGIN(S_ARGLIST);
		return (MDB_TOK_DCMD);
	}

<S_INITIAL>"$<<"|"$<"|"$>"	|
<S_INITIAL>[\$:]{RGX_CMD_CHAR}	{
		/*
		 * Old-style :c or $c command -- lookup the corresponding dcmd
		 * and switch to the argument list state.
		 */
		if ((yylval.l_dcmd = mdb_dcmd_lookup(yytext)) == NULL)
			yyperror("invalid command '%s'", yytext);

		BEGIN(S_ARGLIST);
		return (MDB_TOK_DCMD);
	}

<S_INITIAL>">/"[a-zA-Z0-9]"/"	{
		/*
		 * Variable assignment with size cast -- append the cast letter
		 * to the argument list, and switch to the argument list state.
		 */
		mdb_arg_t arg;

		arg.a_un.a_char = yytext[2];
		arg.a_type = MDB_TYPE_CHAR;

		mdb_argvec_append(&yyargv, &arg);
		yylval.l_dcmd = mdb_dcmd_lookup(">");

		BEGIN(S_ARGLIST);
		return (MDB_TOK_DCMD);
	}

<S_INITIAL>">"	{
		/*
		 * Variable assignment -- switch to the argument list state.
		 */
		yylval.l_dcmd = mdb_dcmd_lookup(yytext);
		BEGIN(S_ARGLIST);
		return (MDB_TOK_DCMD);
	}

<S_INITIAL>[/\\?][ \t]*[vwWZlLM]	{
		/*
		 * Format verb followed by write or match signifier -- switch
		 * to the value list state and return the verb character.  We
		 * also append the actual format character to the arg list.
		 */
		mdb_arg_t arg;

		arg.a_un.a_char = yytext[yyleng - 1];
		arg.a_type = MDB_TYPE_CHAR;

		mdb_argvec_append(&yyargv, &arg);

		BEGIN(S_ARGLIST);
		return yytext[0];
	}

<S_INITIAL>[/\\?=]	{
		/*
		 * Format verb -- switch to the format list state and return
		 * the actual verb character verbatim.
		 */
		BEGIN(S_FMTLIST);
		return (yytext[0]);
	}

<S_INITIAL>'{RGX_CHR_SEQ}$	|
<S_EXPR>'{RGX_CHR_SEQ}$		yyerror("syntax error: ' unmatched");

<S_INITIAL>'{RGX_CHR_SEQ}' 	|
<S_EXPR>'{RGX_CHR_SEQ}'		{
		char *s, *p, *q;
		size_t nbytes;

		/*
		 * If the character sequence is zero-length, return 0.
		 */
		if (yyleng == 2) {
			yylval.l_immediate = 0;
			return (MDB_TOK_IMMEDIATE);
		}

		s = yytext + 1;			/* Skip past initial quote */
		yytext[yyleng - 1] = '\0';	/* Overwrite final quote */
		nbytes = stresc2chr(s);		/* Convert escapes */
		yylval.l_immediate = 0;		/* Initialize token value */

		if (nbytes > sizeof (uintmax_t)) {
			yyerror("character constant may not exceed %lu bytes\n",
			    (ulong_t)sizeof (uintmax_t));
		}

#ifdef _LITTLE_ENDIAN
		p = ((char*)&yylval.l_immediate) + nbytes - 1;

		for (q = s; nbytes != 0; nbytes--)
			*p-- = *q++;
#else
		bcopy(s, ((char *)&yylval.l_immediate) +
		    sizeof (uintmax_t) - nbytes, nbytes);
#endif
		return (MDB_TOK_IMMEDIATE);
	}

\"{RGX_STR_SEQ}$	yyerror("syntax error: \" unmatched");

\"{RGX_STR_SEQ}\"	{
		/*
		 * Quoted string -- convert C escape sequences and return the
		 * string as a token.
		 */
		yylval.l_string = strndup(yytext + 1, yyleng - 2);
		(void) stresc2chr(yylval.l_string);
		return (MDB_TOK_STRING);
	}

<S_ARGLIST>"$["	|
<S_FMTLIST>"$["	{
		/*
		 * Start of expression -- begin expression state and save the
		 * current state so we can return at the end of the expression.
		 */
		oldstate = YYSTATE;
		BEGIN(S_EXPR);
		return (MDB_TOK_LEXPR);
	}

<S_ARGLIST>{RGX_SIMPLE_CHAR}*("'"{RGX_CHR_SEQ}"'"|\"{RGX_STR_SEQ}\"|{RGX_SIMPLE_CHAR}+)* {
		/*
		 * String token -- create a copy of the string and return it.
		 * We need to handle embedded single and double-quote pairs,
		 * which overcomplicates this slightly.
		 */
		yylval.l_string = strdup(yytext);
		string_unquote(yylval.l_string);
		return (MDB_TOK_STRING);
	}

<S_FMTLIST>[0-9]+	{
		/*
		 * Immediate value -- in the format list, all immediates
		 * are assumed to be in decimal.
		 */
		yylval.l_immediate = strtonum(yytext, 10);
		return (MDB_TOK_IMMEDIATE);
	}

<S_FMTLIST>{RGX_SIMPLE_CHAR}	{
		/*
		 * Non-meta character -- in the format list, we return each
		 * character as a separate token to be added as an argument.
		 */
		yylval.l_char = yytext[0];
		return (MDB_TOK_CHAR);
	}

<S_EXPR>";"|"!"|\n	{
		/*
		 * In the expression state only, we cannot see a command
		 * delimiter or shell escape before we end the expression.
		 */
		yyerror("syntax error: $[ unmatched");
	}

<S_EXPR>"]"	{
		/*
		 * End of expression state.  Restore the state we were in
		 * before the "$[" which started this expression.
		 */
		BEGIN(oldstate);
		return (MDB_TOK_REXPR);
	}

<S_INITIAL>"<"{RGX_SYMBOL}	|
<S_INITIAL>"<"[0-9]		|
<S_EXPR>"<"{RGX_SYMBOL}		|
<S_EXPR>"<"[0-9]	{
		/*
		 * Variable reference -- lookup the variable and return a
		 * pointer to it.  Referencing undefined variables is an error.
		 */
		yylval.l_var = mdb_nv_lookup(&mdb.m_nv, &yytext[1]);

		if (yylval.l_var == NULL)
			yyerror("variable '%s' is not defined", &yytext[1]);

		return (MDB_TOK_VAR_REF);
	}

<S_INITIAL>"<<"	|
<S_EXPR>"<<"	return (MDB_TOK_LSHIFT); /* Logical shift left operator */

<S_INITIAL>">>"	|
<S_EXPR>">>"	return (MDB_TOK_RSHIFT); /* Logical shift right operator */

<S_INITIAL>"*/"[a-zA-Z0-9]"/"	|
<S_EXPR>"*/"[a-zA-Z0-9]"/"	{
		switch (yytext[2]) {
			case 'c': case '1':
				return (MDB_TOK_COR1_DEREF);
			case 's': case '2':
				return (MDB_TOK_COR2_DEREF);
			case 'i': case '4':
#ifdef _ILP32
			case 'l':
#endif
				return (MDB_TOK_COR4_DEREF);
#ifdef _LP64
			case 'l':
#endif
			case '8':
				return (MDB_TOK_COR8_DEREF);
		}
		yyerror("invalid cast -- %s\n", yytext);
	}

<S_INITIAL>"%/"[a-zA-Z0-9]"/"	|
<S_EXPR>"%/"[a-zA-Z0-9]"/"	{
		switch (yytext[2]) {
			case 'c': case '1':
				return (MDB_TOK_OBJ1_DEREF);
			case 's': case '2':
				return (MDB_TOK_OBJ2_DEREF);
			case 'i': case '4':
#ifdef _ILP32
			case 'l':
#endif
				return (MDB_TOK_OBJ4_DEREF);
#ifdef _LP64
			case 'l':
#endif
			case '8':
				return (MDB_TOK_OBJ8_DEREF);
		}
		yyerror("invalid cast -- %s\n", yytext);
	}

<S_INITIAL>0[iI][0-1]+	|
<S_EXPR>0[iI][0-1]+	{
		/*
		 * Binary immediate value.
		 */
		yylval.l_immediate = strtonum(yytext + 2, 2);
		return (MDB_TOK_IMMEDIATE);
	}

<S_INITIAL>0[oO][0-7]+	|
<S_EXPR>0[oO][0-7]+	{
		/*
		 * Octal immediate value.
		 */
		yylval.l_immediate = strtonum(yytext + 2, 8);
		return (MDB_TOK_IMMEDIATE);
	}

<S_INITIAL>0[tT][0-9]+"."[0-9]+	|
<S_EXPR>0[tT][0-9]+"."[0-9]+	{
		/*
		 * Decimal floating point value.
		 */
		char *p, c;
		double d;
		int i;

		if ((p = strsplit(yytext, '.')) == NULL)
			yyerror("internal scanning error -- expected '.'\n");

		d = (double)strtonum(yytext + 2, 10);

		for (i = 0; (c = *p++) != '\0'; i++)
			d = d * 10 + c - '0';

		while (i-- != 0)
			d /= 10;

                yylval.l_immediate = *((uintmax_t *)&d);
		return (MDB_TOK_IMMEDIATE);
	}

<S_INITIAL>0[tT][0-9]+	|
<S_EXPR>0[tT][0-9]+	{
		/*
		 * Decimal immediate value.
		 */
		yylval.l_immediate = strtonum(yytext + 2, 10);
		return (MDB_TOK_IMMEDIATE);
	}

<S_INITIAL>0[xX][0-9a-fA-F]+	|
<S_EXPR>0[xX][0-9a-fA-F]+	{
		/*
		 * Hexadecimal value.
		 */
		yylval.l_immediate = strtonum(yytext + 2, 16);
		return (MDB_TOK_IMMEDIATE);
	}

<S_INITIAL>[0-9a-fA-F]+	|
<S_EXPR>[0-9a-fA-F]+	{
		GElf_Sym sym;
		/*
		 * Immediate values without an explicit base are converted
		 * using the default radix (user configurable).  However, if
		 * the token does *not* begin with a digit, it is also a
		 * potential symbol (e.g. "f") so we have to check that first.
		 */
		if (strchr("0123456789", yytext[0]) == NULL &&
		    mdb_tgt_lookup_by_name(mdb.m_target,
		    MDB_TGT_OBJ_EVERY, yytext, &sym) == 0)
			yylval.l_immediate = (uintmax_t)sym.st_value;
		else
			yylval.l_immediate = strtonum(yytext, mdb.m_radix);
		return (MDB_TOK_IMMEDIATE);
	}

<S_INITIAL>{RGX_SYMBOL}	|
<S_EXPR>{RGX_SYMBOL}	{
		/*
		 * Symbol -- parser will look up in symbol table.
		 */
		yylval.l_string = strdup(yytext);
		return (MDB_TOK_SYMBOL);
	}

";"|\n	{
		/*
		 * End of command -- return to start state and return literal.
		 */
		BEGIN(S_INITIAL);
		return (yytext[0]);
	}

[ \t]	;			/* Ignore whitespace */

.	return (yytext[0]);	/* Return anything else */

%%

void
mdb_lex_debug(int i)
{
	yydebug = i;
}

void
mdb_lex_reset(void)
{
	BEGIN(S_INITIAL);
}

void
yydiscard(void)
{
	mdb_iob_discard(mdb.m_in);
	BEGIN(S_INITIAL);
}

static void
yyerror_reset(void)
{
	yydiscard();
	mdb_argvec_reset(&yyargv);
	longjmp(mdb.m_frame->f_pcb, MDB_ERR_PARSE);
}

void
yyerror(const char *format, ...)
{
	va_list alist;
	char *s;

	mdb_iob_puts(mdb.m_err, "mdb: ");
	va_start(alist, format);
	mdb_iob_vprintf(mdb.m_err, format, alist);
	va_end(alist);

	if (strchr(format, '\n') == NULL) {
		if (!mdb_iob_isatty(mdb.m_in)) {
			mdb_iob_printf(mdb.m_err, " on line %d of %s",
			    yylineno, mdb_iob_name(mdb.m_in));
		}

		s = strchr2esc(yytext);
		mdb_iob_printf(mdb.m_err, " near \"%s\"\n", s);
		strfree(s);
	}

	yyerror_reset();
}

void
yyperror(const char *format, ...)
{
	va_list alist;

	va_start(alist, format);
	vwarn(format, alist);
	va_end(alist);

	yyerror_reset();
}

int
yywrap(void)
{
	mdb_dprintf(MDB_DBG_PARSER, "yywrap at line %d\n", yylineno);
	return (1); /* indicate that lex should return a zero token for EOF */
}

/*PRINTFLIKE2*/
/*ARGSUSED*/
int
yyfprintf(FILE *stream, const char *format, ...)
{
	va_list alist;

	va_start(alist, format);
	mdb_iob_vprintf(mdb.m_err, format, alist);
	va_end(alist);
	return (0);
}

/*PRINTFLIKE1*/
int
yyprintf(const char *format, ...)
{
	va_list alist;

	va_start(alist, format);
	mdb_iob_vprintf(mdb.m_err, format, alist);
	va_end(alist);
	return (0);
}

static int
input(void)
{
	int c = mdb_iob_getc(mdb.m_in);

	if (c == '\n')
		yylineno++;

	return (c == EOF ? 0 : c);
}

static void
unput(int c)
{
	if (c == '\n')
		yylineno--;

	(void) mdb_iob_ungetc(mdb.m_in, c == 0 ? EOF : c);
}

static void
output(int c)
{
	char ch = c;
	mdb_iob_nputs(mdb.m_out, &ch, sizeof (ch));
}

static char *
string_nextquote(char *s, char q1, char q2)
{
	char c = 0;

	do {
		if (c != '\\' && (*s == q1 || *s == q2))
			return (s);
	} while ((c = *s++) != '\0');

	return (NULL);
}

static void
string_unquote(char *s)
{
	char *o, *p, *q, c;

	for (o = p = s; (p = string_nextquote(p, '\'', '"')) != NULL; o = p) {
		/*
		 * If the quote wasn't the first character, advance
		 * the destination buffer past what we skipped.
		 */
		if (p > o) {
			(void) strncpy(s, o, p - o);
			s += p - o;
		}

		c = *p;	/* Save the current quote */

		/*
		 * Look ahead and find the matching quote.  If none is
		 * found, use yyerror to longjmp out of the lexer.
		 */
		if (c == '"')
			q = string_nextquote(p + 1, c, c);
		else
			q = strchr(p + 1, c);

		if (q == NULL)
			yyerror("syntax error: %c unmatched", c);

		/*
		 * If the string is non-empty, copy it to the destination
		 * and convert escape sequences if *p is double-quote.
		 */
		if (q > p + 1) {
			(void) strncpy(s, p + 1, q - p - 1);
			if (c == '"') {
                                s[q - p - 1] = '\0';
				s += stresc2chr(s);
			} else
				s += q - p - 1;
		}

		p = q + 1; /* Advance p past matching quote */
	}

	(void) strcpy(s, o);
}
