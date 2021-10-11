/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)edit.c	1.1	99/01/11 SMI"

/*
 *	cscope - interactive C symbol cross-reference
 *
 *	file editing functions
 */

#include <curses.h>	/* KEY_BREAK and refresh */
#include <libgen.h>
#include <stdio.h>
#include "global.h"


/* edit this displayed reference */

void
editref(int i)
{
	char	file[PATHLEN + 1];	/* file name */
	char	linenum[NUMLEN + 1];	/* line number */

	/* verify that there is a references found file */
	if (refsfound == NULL) {
		return;
	}
	/* get the selected line */
	seekline(i + topline);

	/* get the file name and line number */
	if (fscanf(refsfound, "%s%*s%s", file, linenum) == 2) {
		edit(file, linenum);	/* edit it */
	}
	seekline(topline);	/* restore the line pointer */
}

/* edit all references */

void
editall(void)
{
	char	file[PATHLEN + 1];	/* file name */
	char	linenum[NUMLEN + 1];	/* line number */
	int	c;

	/* verify that there is a references found file */
	if (refsfound == NULL) {
		return;
	}
	/* get the first line */
	seekline(1);

	/* get each file name and line number */
	while (fscanf(refsfound, "%s%*s%s%*[^\n]", file, linenum) == 2) {
		edit(file, linenum);	/* edit it */
		if (editallprompt == YES) {
			putmsg("Type ^D to stop editing all lines, "
			    "or any other character to continue: ");
			if ((c = mygetch()) == EOF || c == ctrl('D') ||
			    c == ctrl('Z') || c == KEY_BREAK) {
				/* needed for interrupt on first time */
				(void) refresh();
				break;
			}
		}
	}
	seekline(topline);
}

/* call the editor */

void
edit(char *file, char *linenum)
{
	char	msg[MSGLEN + 1];	/* message */
	char	plusnum[NUMLEN + 2];	/* line number option */
	char	*s;

	(void) sprintf(msg, "%s +%s %s", editor, linenum, file);
	putmsg(msg);
	(void) sprintf(plusnum, "+%s", linenum);

	/* if this is the more or page commands */
	if (strcmp(s = basename(editor), "more") == 0 ||
	    strcmp(s, "page") == 0) {
		/*
		 * get it to pause after displaying a file smaller
		 * than the screen length
		 */
		(void) execute(editor, editor, plusnum, file, "/dev/null",
		    (char *)NULL);
	} else {
		(void) execute(editor, editor, plusnum, file, (char *)NULL);
	}
}
