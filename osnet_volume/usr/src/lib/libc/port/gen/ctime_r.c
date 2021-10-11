/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ctime_r.c	1.2	95/08/24 SMI"	/* SVr4.0 1.14	*/

/*LINTLIBRARY*/

/*
 * This routine converts time as follows.
 * The epoch is 0000 Jan 1 1970 GMT.
 * The argument time is in seconds since then.
 * The localtime(t) entry returns a pointer to an array
 * containing
 *  seconds (0-59)
 *  minutes (0-59)
 *  hours (0-23)
 *  day of month (1-31)
 *  month (0-11)
 *  year-1970
 *  weekday (0-6, Sun is 0)
 *  day of the year
 *  daylight savings flag
 *
 * The routine corrects for daylight saving
 * time and will work in any time zone provided
 * "timezone" is adjusted to the difference between
 * Greenwich and local standard time (measured in seconds).
 * In places like Michigan "daylight" must
 * be initialized to 0 to prevent the conversion
 * to daylight time.
 * There is a table which accounts for the peculiarities
 * undergone by daylight time in 1974-1975.
 *
 * The routine does not work
 * in Saudi Arabia which runs on Solar time.
 *
 * asctime(tvec)
 * where tvec is produced by localtime
 * returns a ptr to a character string
 * that has the ascii time in the form
 *	Thu Jan 01 00:00:00 1970\n\0
 *	01234567890123456789012345
 *	0	  1	    2
 *
 * ctime(t) just calls localtime, then asctime.
 *
 * tzset() looks for an environment variable named
 * TZ.
 * If the variable is present, it will set the external
 * variables "timezone", "altzone", "daylight", and "tzname"
 * appropriately. It is called by localtime, and
 * may also be called explicitly by the user.
 */

#pragma weak ctime_r = _ctime_r

#include "synonyms.h"
#include <time.h>
#include <sys/types.h>
#include <errno.h>
#include <thread.h>
#include <synch.h>
#include <mtlib.h>
#include "libc.h"


/*
 * POSIX.1c Draft-6 version of the function ctime_r.
 * It was implemented by Solaris 2.3.
 */
char *
ctime_r(const time_t *t, char *buffer, int buflen)
{
	struct tm res;

	if (localtime_r(t, &res) == NULL)
		return (NULL);

	if (asctime_r(&res, buffer, buflen) == NULL)
		return (NULL);

	return (buffer);
}

/*
 * POSIX.1c standard version of the function ctime_r.
 * User gets it via static ctime_r from the header file.
 */
char *
__posix_ctime_r(const time_t *t, char *buffer)
{
	struct tm res;

	if (localtime_r(t, &res) == NULL)
		return (NULL);

	if (__posix_asctime_r(&res, buffer) == NULL)
		return (NULL);

	return (buffer);
}
