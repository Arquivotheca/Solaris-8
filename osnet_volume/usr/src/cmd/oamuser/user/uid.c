/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)uid.c	1.6	96/09/04 SMI"	/* SVr4.0 1.5 */

#include <sys/types.h>
#include <stdio.h>
#include <userdefs.h>

#include <sys/param.h>
#ifndef	MAXUID
#include <limits.h>
#ifdef UID_MAX
#define	MAXUID	UID_MAX
#else
#define	MAXUID	60000
#endif
#endif

/*
 * Check to see that the uid is not a reserved uid
 * -- nobody, noaccess or nobody4
 */
static int
isvaliduid(uid_t uid)
{
	return (uid != 60001 && uid != 60002 && uid != 65534);
}

uid_t
findnextuid()
{
	FILE *fptr;
	uid_t last, next;
	uid_t uid;

	/*
	 * Sort the used UIDs in decreasing order to return MAXUSED + 1
	 */
	if ((fptr = popen("exec sh -c "
	    "\"getent passwd|cut -f3 -d:|sort -nr|uniq\" 2>/dev/null",
	    "r")) == NULL)
		return (-1);

	if (fscanf(fptr, "%ld\n", &next) == EOF) {
		(void) pclose(fptr);
		return (DEFRID + 1);
	}

	/*
	 * 'next' is now the highest allocated uid.
	 *
	 * The simplest allocation is where we just add one, and obtain
	 * a valid uid.  If this fails look for a hole in the uid range ..
	 */

	last = MAXUID;		/* upper limit */
	uid = -1;		/* start invalid */
	do {
		if (!isvaliduid(next))
			continue;

		if (next <= DEFRID) {
			if (last != DEFRID + 1)
				uid = DEFRID + 1;
			break;
		}

		if ((uid = next + 1) != last) {
			while (!isvaliduid(uid))
				uid++;
			if (uid > 0 && uid < last)
				break;
		}

		uid = -1;
		last = next;

	} while (fscanf(fptr, "%ld\n", &next) != EOF);

	(void) pclose(fptr);

	return (uid);
}
