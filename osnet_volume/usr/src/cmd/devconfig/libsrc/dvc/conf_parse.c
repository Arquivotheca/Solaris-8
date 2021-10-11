/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)conf_parse.c 1.6 95/02/28 SMI"

#include <stdio.h>
#include <ctype.h>
#include <libintl.h>
#include <string.h>

#include "dvc.h"
#include "dvc_msgs.h"
#include "conf.h"
#include "util.h"

static char *val_bfr;
static int   val_idx;
static int   val_size = 30;

static void
str_add(char ch)
{
	if (val_bfr == NULL)
		val_bfr = (char *)xmalloc(val_size);

	if (val_idx == val_size)
		val_bfr = (char *)xrealloc(val_bfr, val_size *= 2);

	val_bfr[val_idx++] = ch;
}

static char *
str_end()
{
	str_add('\0');
	val_idx = 0;

	return (val_bfr);
}

/*
 * Get a decimal, hex or octal number, for unsigned integers.  In most cases
 * this will be for hex or octal addresses.  The parameter token must point
 * to a valid (zero terminated) numeric string.
 */

static unsigned int
convert_unumeric_value(char *token)
{
	unsigned int  radix = 10;
	unsigned int  retval = 0;
	char c;

	c = *token;
	if (c == '0') {
		++token;
		c = *token;

		if (c == 'x' || c == 'X') {
			++token;
			c = *token;
			radix = 16;
		} else
			radix = 8;
	}

	while (c) {
		if ('a' <= c && c <= 'f')
			c = c - 'a' + 10;
		else if ('A' <= c && c <= 'F')
			c = c - 'A' + 10;
		else
			c -= '0';
	/* Issue warning if we read a bogus digit. */
		if (c >= radix)
			vrb(MSG(BADDIGIT), *token, radix);
		retval = (retval * radix) + c;
		++token;
		c = *token;
	}

	return (retval);
}

	/*
	 * Get a decimal, hex or octal number.  The parameter token must point
	 * to a valid (zero terminated) numeric string.  Becasue of
	 */

static int
convert_numeric_value(char *token)
{
	int  radix = 10;
	int  retval = 0;
	char c;

	c = *token;
	if (c == '0') {
		++token;
		c = *token;

		if (c == 'x' || c == 'X') {
			++token;
			c = *token;
			radix = 16;
		} else
			radix = 8;
	}

	while (c) {
		if ('a' <= c && c <= 'f')
			c = c - 'a' + 10;
		else if ('A' <= c && c <= 'F')
			c = c - 'A' + 10;
		else
			c -= '0';

		/* Issue warning if we read a bogus digit. */
		if (c >= radix)
			vrb(MSG(BADDIGIT), *token, radix);

		retval = (retval * radix) + c;

		++token;
		c = *token;
	}

	return (retval);
}

typedef enum {
	COMMA,
	COMPLEMENT,
	ENDOFFILE,
	EQUALS,
	MINUS,
	NAME,
	NUMERIC,
	UNUMERIC,
	SEMICOLON,
	STRING,
	UNKNOWN
} token_t;

static char *tokennames[] = {
	"COMMA",
	"COMPLEMENT",
	"ENDOFFILE",
	"EQUALS",
	"MINUS",
	"NAME",
	"NUMERIC",
	"UNUMERIC",
	"SEMICOLON",
	"STRING",
	"UNKNOWN"
};

static int
isnamechar(char ch)
{
	switch (ch) {
		case '"':
		case '#':
		case ',':
		case ';':
		case '=':
		case '~':
			return (0);
	}
	return (isgraph(ch));
}

static int
white(char ch)
{
	switch (ch) {
	case ' ':
	case '\t':
	case '\n':
	case ',':
	case ';':
		return (TRUE);
	}
	return (FALSE);
}

static int
is_number(char *str)
{
	int i;

	for (i = 0; !white(*str); ++i, ++str) {
		char ch = *str;

		/* Could be a hex number! */
		if (((ch == 'x') || (ch == 'X')) && i == 1)
			continue;

		if (('0' <= ch) && (ch <= '9'))
			continue;

		if ((('a' <= ch) && (ch <= 'f')) ||
		    (('A' <= ch) && (ch <= 'F')))
			continue;

		return (FALSE);
	}

	return (TRUE);
}

static token_t
lex(char **filep, char **val)
{
	char *file = *filep;
	char    ch;
	token_t token;

lex_start:
	/*LINTED*/
	while ((ch = *file++) && isspace(ch))
		;

	switch (ch) {
	/* Comment to end of line */
	case '#':
		while ((ch = *file++))
			if (ch == '\n')
				goto lex_start;
		/*FALLTHRU*/


	/* End of file */
	case '\0':
		token = ENDOFFILE;
		break;

	/* Single character tokens. */
	case ',':	token = COMMA;		break;
	case '-':	token = MINUS;		break;
	case ';':	token = SEMICOLON;	break;
	case '=':	token = EQUALS;		break;
	case '~':	token = COMPLEMENT;	break;

	/* String */
	case '"':
		while (((ch = *file++) != '"') && ch) {
			/* Escaping newlines is optional. */
			if (ch == '\\' && *file == '\n') {
				++file;
				continue;
			}
			str_add(ch);
		}
		token = STRING;
		break;

	/* Number or name */
	default:
		str_add(ch);
		if (is_number(file-1)) {
			if (ch == '0') {
				ch = *file++;
				if (ch == 'x' || ch == 'X') {
					str_add(ch);
					ch = *file++;
					while (isxdigit(ch)) {
						str_add(ch);
						ch = *file++;
					}
					token = UNUMERIC;
				} else {
					while ('0' <= ch && ch <= '7') {
						str_add(ch);
						ch = *file++;
					}
					token = UNUMERIC;
				}
			} else {
				ch = *file++;
				while (isdigit(ch)) {
					str_add(ch);
					ch = *file++;
				}
				token = NUMERIC;
			}
			--file;


		} else if (isnamechar(ch)) {
			ch = *file++;
			while (isnamechar(ch)) {
				str_add(ch);
				ch = *file++;
			}
			--file;
			token = NAME;
		} else {
			*filep = file;
			token = UNKNOWN;
		}
		break;
	}

	*val = str_end();
	*filep = file;
	return (token);
}

static val_list_t *
get_value(char **fcp, char **val, token_t *token)
{
	val_list_t *vp = NULL;
	val_list_t *vplast = NULL;

top:
	vp = (val_list_t *)xmalloc(sizeof (*vp));
	vp->next = vplast;

	switch (*token) {
	case MINUS:
		if ((*token = lex(fcp, val)) == NUMERIC) {
			vp->val_type = VAL_NUMERIC;
			vp->val.integer = -convert_numeric_value(*val);
		} else
			vrb(MSG(NUMERIC1));
		break;

	case COMPLEMENT:
		if ((*token = lex(fcp, val)) == NUMERIC) {
			vp->val_type = VAL_NUMERIC;
			vp->val.integer = ~convert_numeric_value(*val);
		} else
			vrb(MSG(NUMERIC2));
		break;

	case NUMERIC:
		vp->val_type = VAL_NUMERIC;
		vp->val.integer = convert_numeric_value(*val);
		break;

	case UNUMERIC:
		vp->val_type = VAL_UNUMERIC;
		vp->val.uinteger = convert_unumeric_value(*val);
		break;

	case NAME:
		/*
		 * Names (without quotes) will be taken as strings in this
		 * context.  Not a good feature to rely upon, this syntax
		 * is rejected by the Solaris kernel.
		 *
		 * Fall through...
		 */

	case STRING:
		vp->val_type = VAL_STRING;
		vp->val.string = dgettext(DVC_MSGS_TEXTDOMAIN, xstrdup(*val));
		break;

	default:
		xfree(vp);
		vrb(MSG(NUMERIC3));
		return (NULL);
	}

	if ((*token = lex(fcp, val)) == COMMA) {
		*token = lex(fcp, val);
		vplast = vp;
		goto top;
	}

	return (vp);
}

static attr_list_t *
get_attr(char **fcp, char **val, token_t *token)
{
	attr_list_t *al = NULL;

	while (*token == NAME) {
		attr_list_t *newal = (attr_list_t *)
			    xmalloc(sizeof (attr_list_t));
		newal->next = al;
		al = newal;

		al->name = xstrdup(*val);

		*token = lex(fcp, val);
		if (*token == EQUALS) {
			*token = lex(fcp, val);
			al->vlist = get_value(fcp, val, token);
		} else {
			vrb(MSG(EXPECTED));
			al->vlist = NULL;
		}
	}

	return (al);
}

conf_list_t *
parse_conf(char *fcp)
{
	int done = 0;
	token_t token;
	char *val;

	conf_list_t *cf = NULL;
	attr_list_t *al = NULL;

	token = lex(&fcp, &val);
	while (!done) {
		switch (token) {
		case ENDOFFILE:
			++done;
			/*FALLTHRU*/

		case SEMICOLON: {
			token = lex(&fcp, &val);
			break;
		}

		case NAME:
			al = get_attr(&fcp, &val, &token);
			if (al) {
				conf_list_t *newcf = (conf_list_t *)
					    xmalloc(sizeof (conf_list_t));
				newcf->next = cf;
				cf = newcf;
				cf->alist = al;
			}
			al = NULL;
			break;

		default:
			vrb(MSG(BADTOKEN), tokennames[token], val);
			token = lex(&fcp, &val);
			break;
		}
	}

	return (cf);
}
