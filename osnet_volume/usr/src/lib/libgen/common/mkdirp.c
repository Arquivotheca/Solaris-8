/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mkdirp.c	1.10	97/03/28 SMI"

/*LINTLIBRARY*/

#pragma weak mkdirp = _mkdirp

/*
 * Creates directory and it's parents if the parents do not
 * exist yet.
 *
 * Returns -1 if fails for reasons other than non-existing
 * parents.
 * Does NOT simplify pathnames with . or .. in them.
 */

#include "synonyms.h"
#include <sys/types.h>
#include <libgen.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

static char *simplify(const char *str);

int
mkdirp(const char *d, mode_t mode)
{
	char  *endptr, *ptr, *slash, *str;

	str = simplify(d);

	/* If space couldn't be allocated for the simplified names, return. */

	if (str == NULL)
		return (-1);

		/* Try to make the directory */

	if (mkdir(str, mode) == 0) {
		free(str);
		return (0);
	}
	if (errno != ENOENT) {
		free(str);
		return (-1);
	}
	endptr = strrchr(str, '\0');
	slash = strrchr(str, '/');

		/* Search upward for the non-existing parent */

	while (slash != NULL) {

		ptr = slash;
		*ptr = '\0';

			/* If reached an existing parent, break */

		if (access(str, 00) == 0)
			break;

			/* If non-existing parent */

		else {
			slash = strrchr(str, '/');

				/* If under / or current directory, make it. */

			if (slash == NULL || slash == str) {
				if (mkdir(str, mode)) {
					free(str);
					return (-1);
				}
				break;
			}
		}
	}

	/* Create directories starting from upmost non-existing parent */

	while ((ptr = strchr(str, '\0')) != endptr) {
		*ptr = '/';
		if (mkdir(str, mode) < 0 && errno != EEXIST) {
			/*
			 *  If the mkdir fails because str already
			 *  exists (EEXIST), then str has the form
			 *  "existing-dir/..", and this is really
			 *  ok. (Remember, this loop is creating the
			 *  portion of the path that didn't exist)
			 */
			free(str);
			return (-1);
		}
	}
	free(str);
	return (0);
}

/*
 *	simplify - given a pathname, simplify that path by removing
 *		   duplicate contiguous slashes.
 *
 *		   A simplified copy of the argument is returned to the
 *		   caller, or NULL is returned on error.
 *
 *		   The caller should handle error reporting based upon the
 *		   returned vlaue, and should free the returned value,
 *		   when appropriate.
 */

static char *
simplify(const char *str)
{
	int i;
	size_t mbPathlen;	/* length of multi-byte path */
	size_t wcPathlen;	/* length of wide-character path */
	wchar_t *wptr;		/* scratch pointer */
	wchar_t *wcPath;	/* wide-character version of the path */
	char *mbPath;		/* The copy fo the path to be returned */

	/*
	 *  bail out if there is nothing there.
	 */

	if (!str)
	    return (NULL);

	/*
	 *  Get a copy of the argument.
	 */

	if ((mbPath = strdup(str)) == NULL) {
		return (NULL);
	}

	/*
	 *  convert the multi-byte version of the path to a
	 *  wide-character rendering, for doing our figuring.
	 */

	mbPathlen = strlen(mbPath);

	if ((wcPath = calloc(sizeof (wchar_t), mbPathlen+1)) == NULL) {
		free(mbPath);
		return (NULL);
	}

	if ((wcPathlen = mbstowcs(wcPath, mbPath, mbPathlen)) == -1) {
		free(mbPath);
		free(wcPath);
		return (NULL);
	}

	/*
	 *  remove duplicate slashes first ("//../" -> "/")
	 */

	for (wptr = wcPath, i = 0; i < wcPathlen; i++) {
		*wptr++ = wcPath[i];

		if (wcPath[i] == '/') {
			i++;

			while (wcPath[i] == '/') {
				i++;
			}

			i--;
		}
	}

	*wptr = '\0';

	/*
	 *  now convert back to the multi-byte format.
	 */

	if (wcstombs(mbPath, wcPath, mbPathlen) == -1) {
		free(mbPath);
		free(wcPath);
		return (NULL);
	}

	free(wcPath);
	return (mbPath);
}
