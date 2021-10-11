/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * open.c -- routines to open files
 */

#ident	"<@(#)open.c	1.8	98/03/12 SMI>"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <direct.h>

#include "debug.h"
#include "dir.h"
#include "err.h"
#include "open.h"

/*
 * init_open -- initialize the open module and cd to root of boot tree
 */

void
init_open()
{
	/*
	 * this may fail, but then that might be okay because we're being
	 * run from the appropriate directory already.  if not, the user
	 * will see a fatal error as soon as we try to open config files
	 * like devicedb/master...
	 */
	_chdir("/boot");
}

/*
 * fn_open -- build the name of a file and try to open it
 *
 * the pathname in "name" is modified as follows:
 *	if "newdirname" is not NULL, it replaces the directory part of "name"
 *	if "newsuffix" is not NULL, it replaces the extension in "name"
 *	if "namebuf" is not NULL, the resulting name is returned in it
 *
 * NOTE: under DOS, the O_BINARY flag is always added automatically to the
 *       open() call.
 */

int
fn_open(char *namebuf, unsigned len, const char *newdirname, const char *name,
    const char *newsuffix, int flags)
{
	const char *lastcomp;	/* the last component of the name */
	const char *dot;	/* dot in last component */
	int fd;			/* fd from open call */
	int called_malloc = 0;	/* true if we malloc'd namebuf */
	char *enamebuf;		/* limit when copying into namebuf */
	char *ptr;		/* next available char in namebuf */

	/* if user didn't supply a namebuf, allocate one here */
	if (namebuf == NULL) {
		called_malloc = 1;
		len = PATH_MAX;
		if ((namebuf = malloc(len)) == NULL)
			MemFailure();
	}

	/* sanity check */
	if (len < 4)
		fatal("fn_open: unexpected len %d", len);

	/* fix starting & ending points for copies into namebuf */
	ptr = namebuf;
	enamebuf = &namebuf[len - 3];	/* room for slash, dot, and a NULL */

	/* replace the dirname if caller asked us to */
	if (newdirname) {
		/* copy in new dirname */
		for (; ptr < enamebuf; ptr++, newdirname++)
			if ((*ptr = *newdirname) == '\0')
				break;

		/* make sure there's a slash */
		if ((ptr > namebuf) && (*(ptr - 1) != '/'))
			*ptr++ = '/';

		/* peel old dirname off of name passed in */
		if ((lastcomp = strrchr(name, '/')) != NULL) {
			while (*lastcomp == '/')
				lastcomp++;
			name = lastcomp;
		}
	}

	/* copy in the name, stop at the extension if we're replacing it */
	if (newsuffix)
		dot = strrchr(name, '.');
	else
		dot = NULL;
	for (; ptr < enamebuf; ptr++, name++)
		if ((dot != NULL) && (name == dot))
			break;
		else if ((*ptr = *name) == '\0')
			break;

	/* add the extension if caller asked us to */
	if (newsuffix) {
		*ptr++ = '.';
		for (; ptr < enamebuf; ptr++, newsuffix++)
			if ((*ptr = *newsuffix) == '\0')
				break;
	}

	/* tack on a NULL (in case loop hit enamebuf limit above) */
	*ptr++ = '\0';

	/* go for it */
	fd = _open(namebuf, flags | _O_BINARY);

	/* free up the buffer if it belongs to us */
	if (called_malloc)
		free(namebuf);

	return (fd);
}
