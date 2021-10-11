/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)input.c	1.1	99/01/11 SMI"

/*
 *	cscope - interactive C symbol cross-reference
 *
 *	terminal input functions
 */

#include "global.h"
#include <curses.h>	/* KEY_BACKSPACE, KEY_BREAK, and KEY_ENTER */
#include <setjmp.h>	/* jmp_buf */

static	jmp_buf	env;		/* setjmp/longjmp buffer */
static	int	prevchar;	/* previous, ungotten character */

/* catch the interrupt signal */

/*ARGSUSED*/
SIGTYPE
catchint(int sig)
{
	(void) signal(SIGINT, catchint);
	longjmp(env, 1);
}

/* unget a character */

int
ungetch(int c)
{
	prevchar = c;
	return (0);
}

/* get a character from the terminal */

int
mygetch(void)
{
	SIGTYPE	(*savesig)();		/* old value of signal */
	int	c;

	/* change an interrupt signal to a break key character */
	if (setjmp(env) == 0) {
		savesig = signal(SIGINT, catchint);
		(void) refresh();	/* update the display */
		reinitmouse();	/* curses can change the menu number */
		if (prevchar) {
			c = prevchar;
			prevchar = 0;
		} else {
			c = getch();	/* get a character from the terminal */
		}
	} else {	/* longjmp to here from signal handler */
		c = KEY_BREAK;
	}
	(void) signal(SIGINT, savesig);
	return (c);
}

/* get a line from the terminal in non-canonical mode */

int
getline(char s[], size_t size, int firstchar, BOOL iscaseless)
{
	int	c, i = 0;
	int	j;

	/* if a character already has been typed */
	if (firstchar != '\0') {
		if (iscaseless == YES) {
			firstchar = tolower(firstchar);
		}
		(void) addch((unsigned)firstchar);	/* display it */
		s[i++] = firstchar;	/* save it */
	}
	/* until the end of the line is reached */
	while ((c = mygetch()) != '\r' && c != '\n' && c != KEY_ENTER &&
	    c != KEY_BREAK) {
		if (c == erasechar() || c == KEY_BACKSPACE) {	/* erase */
			if (i > 0) {
				(void) addstr("\b \b");
				--i;
			}
		} else if (c == killchar()) {			/* kill */
			for (j = 0; j < i; ++j) {
				(void) addch('\b');
			}
			for (j = 0; j < i; ++j) {
				(void) addch(' ');
			}
			for (j = 0; j < i; ++j) {
				(void) addch('\b');
			}
			i = 0;
		} else if (isprint(c) || c == '\t') {		/* printable */
			if (iscaseless == YES) {
				c = tolower(c);
			}
			/* if it will fit on the line */
			if (i < size) {
				(void) addch((unsigned)c);	/* display it */
				s[i++] = c;		/* save it */
			}
		} else if (c == ctrl('X')) {
			/* mouse */
			(void) getmouseevent(); 	/* ignore it */
		} else if (c == EOF) {			/* end-of-file */
			break;
		}
		/* return on an empty line to allow a command to be entered */
		if (firstchar != '\0' && i == 0) {
			break;
		}
	}
	s[i] = '\0';
	return (i);
}

/* ask user to enter a character after reading the message */

void
askforchar(void)
{
	(void) addstr("Type any character to continue: ");
	(void) mygetch();
}

/* ask user to press the RETURN key after reading the message */

void
askforreturn(void)
{
	if (linemode == NO) {
		(void) fprintf(stderr, "Press the RETURN key to continue: ");
		(void) getchar();
	}
}

/* expand the ~ and $ shell meta characters in a path */

void
shellpath(char *out, int limit, char *in)
{
	char	*lastchar;
	char	*s, *v;

	/* skip leading white space */
	while (isspace(*in)) {
		++in;
	}
	lastchar = out + limit - 1;

	/*
	 * a tilde (~) by itself represents $HOME; followed by a name it
	 * represents the $LOGDIR of that login name
	 */
	if (*in == '~') {
		*out++ = *in++;	/* copy the ~ because it may not be expanded */

		/* get the login name */
		s = out;
		while (s < lastchar && *in != '/' && *in != '\0' &&
		    !isspace(*in)) {
			*s++ = *in++;
		}
		*s = '\0';

		/* if the login name is null, then use $HOME */
		if (*out == '\0') {
			v = getenv("HOME");
		} else {	/* get the home directory of the login name */
			v = logdir(out);
		}
		/* copy the directory name */
		if (v != NULL) {
			(void) strcpy(out - 1, v);
			out += strlen(v) - 1;
		} else {
			/* login not found so ~ must be part of the file name */
			out += strlen(out);
		}
	}
	/* get the rest of the path */
	while (out < lastchar && *in != '\0' && !isspace(*in)) {

		/* look for an environment variable */
		if (*in == '$') {
			/* copy the $ because it may not be expanded */
			*out++ = *in++;

			/* get the variable name */
			s = out;
			while (s < lastchar && *in != '/' && *in != '\0' &&
			    !isspace(*in)) {
				*s++ = *in++;
			}
			*s = '\0';

			/* get its value */
			if ((v = getenv(out)) != NULL) {
				(void) strcpy(out - 1, v);
				out += strlen(v) - 1;
			} else {
				/*
				 * var not found, so $ must be part of
				 * the file name
				 */
				out += strlen(out);
			}
		} else {	/* ordinary character */
			*out++ = *in++;
		}
	}
	*out = '\0';
}
