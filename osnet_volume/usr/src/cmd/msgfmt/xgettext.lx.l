%{
/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#pragma ident   "@(#)xgettext.lx.l 1.6     99/03/12 SMI"

#include <stdio.h>

#define	TRUE	1
#define	FALSE	0

extern int	stdin_only;
extern char	curr_file[];	/* Name of file currently being read. */
extern int	curr_linenum;	/* line number in the current input file */
extern int	warn_linenum;	/* line number of current warning message */
extern int	optind;
extern int	gargc;
extern char	**gargv;

/*
 * The following lex rule basically wants to recognize tokens
 * that can change the current state of scanning source code.
 * Evertime such tokens are recognized, the specific handler will be
 * executed. All other tokens are treated as regular characters and
 * they are all handled the same way.
 * The tricky part was not to miss any specification in ANSI-C code
 * that looks like a meaningful token but not a meaningful one and
 * should be treated as regular characters.
 * For example,
 *	c= '"';d='"'; printf("\"" "\(\)\\\"");
 *	c = ABgettext("Not a gettext");
 *	c = gettextXY("Not a gettext");
 *	c = ABgettextXY("Not a gettext");
 */
%}

IDCHARS		[a-zA-Z0-9_]

%%
^#(.*\\\n)**.*\n	{ handle_macro_line(); }

\/\/		{ handle_cplus_comment_line(); }

\/\* 		{ handle_open_comment(); }

\*\/ 		{ handle_close_comment(); }

dcgettext	{ handle_dcgettext(); }

dgettext	{ handle_dgettext(); }

gettext		{ handle_gettext(); }

textdomain	{ handle_textdomain(); }

{IDCHARS}+	|
\'\"\'		|
\'\\\"\'	|
\\\\		|
\\\"		|
\\\(		|
\\\)		{ handle_character(); }

\(		{ handle_open_paren(); }

\)		{ handle_close_paren(); }

\\\n		{ handle_esc_newline(); }

\,		{ handle_comma (); }

\n		{ handle_newline(); }

\"		{ handle_quote (); }

" "		{ handle_spaces (); }

"\t"		{ handle_spaces (); }

.		{ handle_character(); }

%%

/*
 * Since multiple files can be processed, yywrap() should keep feeding
 * all input files specified.
 */
yywrap()
{
	FILE	*fp;

	if ((optind >= gargc) || (stdin_only == TRUE)) {
		return (1);
	} else {
		/*
		 * gargv still contains not-processed input files.
		 */
		freopen (gargv[optind], "r", stdin);
		if ((fp = freopen (gargv[optind], "r", stdin)) == NULL) {
			fprintf(stderr, "ERROR, can't open input file: %s\n",
					gargv[optind]);
		}
#ifdef DEBUG
		printf("In yywrap(), opening file  %d, <%s>\n",
				optind, gargv[optind]);
#endif
		/*
		 * Reset global file name and line number for the new file.
		 */
		strcpy (curr_file, gargv[optind]);
		curr_linenum = 0;
		warn_linenum = 0;

		optind++;

		return (0);
	}

} /* yywrap */
