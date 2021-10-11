/*
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)debug.c	1.25	98/08/28 SMI"

#include	<stdio.h>
#include	<string.h>
#include	<stdlib.h>
#include	"_debug.h"
#include	"msg.h"

int		_Dbg_mask;
static int	_Dbg_count = 0;


/*
 * Debugging initialization and processing.  The options structure defines
 * a set of option strings that can be specified using the -D flag or from an
 * environment variable.  For each option, a class is enabled in the _Dbg_mask
 * bit mask.
 */
static DBG_options _Dbg_options[] = {
	{MSG_ORIG(MSG_TOK_ALL),		DBG_ALL},
	{MSG_ORIG(MSG_TOK_ARGS),	DBG_ARGS},
	{MSG_ORIG(MSG_TOK_BASIC),	DBG_BASIC},
	{MSG_ORIG(MSG_TOK_BINDINGS),	DBG_BINDINGS},
	{MSG_ORIG(MSG_TOK_DETAIL),	DBG_DETAIL},
	{MSG_ORIG(MSG_TOK_ENTRY),	DBG_ENTRY},
	{MSG_ORIG(MSG_TOK_FILES),	DBG_FILES},
	{MSG_ORIG(MSG_TOK_HELP),	DBG_HELP},
	{MSG_ORIG(MSG_TOK_LIBS),	DBG_LIBS},
	{MSG_ORIG(MSG_TOK_MAP),		DBG_MAP},
	{MSG_ORIG(MSG_TOK_RELOC),	DBG_RELOC},
	{MSG_ORIG(MSG_TOK_SECTIONS),	DBG_SECTIONS},
	{MSG_ORIG(MSG_TOK_SEGMENTS),	DBG_SEGMENTS},
	{MSG_ORIG(MSG_TOK_SUPPORT),	DBG_SUPPORT},
	{MSG_ORIG(MSG_TOK_SYMBOLS),	DBG_SYMBOLS},
	{MSG_ORIG(MSG_TOK_AUDIT),	DBG_AUDITING},
	{MSG_ORIG(MSG_TOK_VERSIONS),	DBG_VERSIONS},
	{MSG_ORIG(MSG_TOK_GOT),		DBG_GOT},
	{MSG_ORIG(MSG_TOK_MOVE),	DBG_MOVE},
	{NULL,				NULL},
};

/*
 * Provide a debugging usage message
 */
static void
_Dbg_usage()
{
	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_USE_RTLD_A));
	dbg_print(MSG_INTL(MSG_USE_RTLD_B));
	dbg_print(MSG_INTL(MSG_USE_RTLD_C));
	dbg_print(MSG_INTL(MSG_USE_RTLD_D));
	dbg_print(MSG_INTL(MSG_USE_RTLD_E));
	dbg_print(MSG_INTL(MSG_USE_RTLD_F));
	dbg_print(MSG_INTL(MSG_USE_RTLD_G));

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_USE_LD_A));
	dbg_print(MSG_INTL(MSG_USE_LD_B));
	dbg_print(MSG_INTL(MSG_USE_LD_C));
	dbg_print(MSG_INTL(MSG_USE_LD_D));
	dbg_print(MSG_INTL(MSG_USE_LD_E));
	dbg_print(MSG_INTL(MSG_USE_LD_F));

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_USE_ARGS));
	dbg_print(MSG_INTL(MSG_USE_BASIC));
	dbg_print(MSG_INTL(MSG_USE_BINDINGS));
	dbg_print(MSG_INTL(MSG_USE_BINDINGS_2));
	dbg_print(MSG_INTL(MSG_USE_DETAIL));
	dbg_print(MSG_INTL(MSG_USE_ENTRY));
	dbg_print(MSG_INTL(MSG_USE_FILES));
	dbg_print(MSG_INTL(MSG_USE_HELP));
	dbg_print(MSG_INTL(MSG_USE_LIBS));
	dbg_print(MSG_INTL(MSG_USE_LIBS_2));
	dbg_print(MSG_INTL(MSG_USE_MAP));
	dbg_print(MSG_INTL(MSG_USE_MOVE));
	dbg_print(MSG_INTL(MSG_USE_RELOC));
	dbg_print(MSG_INTL(MSG_USE_SECTIONS));
	dbg_print(MSG_INTL(MSG_USE_SEGMENTS));
	dbg_print(MSG_INTL(MSG_USE_SEGMENTS_2));
	dbg_print(MSG_INTL(MSG_USE_SUPPORT));
	dbg_print(MSG_INTL(MSG_USE_SYMBOLS));
	dbg_print(MSG_INTL(MSG_USE_SYMBOLS_2));
	dbg_print(MSG_INTL(MSG_USE_VERSIONS));
	dbg_print(MSG_INTL(MSG_USE_AUDIT));
	dbg_print(MSG_INTL(MSG_USE_GOT));
}

/*
 * Validate and enable the appropriate debugging classes.
 */
int
Dbg_setup(const char * string)
{
	char *		name, * _name;	/* Temporary buffer in which to */
					/* perform strtok() operations. */
	DBG_opts 	opts;		/* Ptr to cycle thru _Dbg_options[]. */
	const char *	comma = MSG_ORIG(MSG_STR_COMMA);

	if ((_name = (char *)malloc(strlen(string) + 1)) == 0)
		return (0);
	(void) strcpy(_name, string);

	/*
	 * The token should be of the form "-Dtok,tok,tok,...".  Separate the
	 * pieces and build up the appropriate mask, unrecognized options are
	 * flagged.
	 */
	if ((name = strtok(_name, comma)) != NULL) {
		Boolean		found, set;
		do {
			found = FALSE;
			set = TRUE;
			if (name[0] == '!') {
				set = FALSE;
				name++;
			}
			for (opts = _Dbg_options; opts->o_name != NULL;
				opts++) {
				if (strcmp(name, opts->o_name) == 0) {
					if (set == TRUE)
						_Dbg_mask |= opts->o_mask;
					else
						_Dbg_mask &= ~(opts->o_mask);
					found = TRUE;
					break;
				}
			}
			if (found == FALSE)
				dbg_print(MSG_INTL(MSG_USE_UNRECOG), name);
		} while ((name = strtok(NULL, comma)) != NULL);
	}
	(void) free(_name);

	/*
	 * If the debug help option was specified dump a usage message.  If
	 * this is the only debug option return an indication that the user
	 * should exit.
	 */
	if ((_Dbg_mask & DBG_HELP) && !_Dbg_count) {
		_Dbg_usage();
		if (_Dbg_mask == DBG_HELP)
			/* LINTED */
			return ((int)S_ERROR);
	}

	_Dbg_count++;

	return (_Dbg_mask);
}


/*
 * Messaging support - funnel everything through _dgettext() as this provides
 * a stub binding to libc, or a real binding to libintl.
 */
extern char *	_dgettext(const char *, const char *);

const char *
_liblddbg_msg(Msg mid)
{
	return (_dgettext(MSG_ORIG(MSG_SUNW_OST_SGS), MSG_ORIG(mid)));
}
