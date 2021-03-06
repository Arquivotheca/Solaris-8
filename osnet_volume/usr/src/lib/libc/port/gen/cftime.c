/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cftime.c	1.10	97/06/21 SMI"	/* SVr4.0 1.15	*/

/*LINTLIBRARY*/

/*
 * This routine converts time as follows.  The epoch is 0000  Jan  1
 * 1970  GMT.   The  argument  time  is  in seconds since then.  The
 * localtime(t) entry returns a pointer to an array containing:
 *
 *		  seconds (0-59)
 *		  minutes (0-59)
 *		  hours (0-23)
 *		  day of month (1-31)
 *		  month (0-11)
 *		  year
 *		  weekday (0-6, Sun is 0)
 *		  day of the year
 *		  daylight savings flag
 *
 * The routine corrects for daylight saving time and  will  work  in
 * any  time  zone provided "timezone" is adjusted to the difference
 * between Greenwich and local standard time (measured in seconds).
 *
 *	 ascftime(buf, format, t)	-> where t is produced by localtime
 *				           and returns a ptr to a character
 *				           string that has the ascii time in
 *				           the format specified by the format
 *				           argument (see date(1) for format
 *				           syntax).
 *
 *	 cftime(buf, format, t) 	-> just calls ascftime.
 *
 *
 *
 */

#pragma weak ascftime = _ascftime
#pragma weak cftime = _cftime

#include	"synonyms.h"
#include	<mtlib.h>
#include	<stddef.h>
#include	<time.h>
#include	<limits.h>
#include	<stdlib.h>
#include	<thread.h>
#include	<synch.h>

int
cftime(char *buf, char *format, const time_t *t)
{
	struct tm res;

	return (ascftime(buf, format, localtime_r(t, &res)));
}

int
ascftime(char *buf, const char *format, const struct tm *tm)
{
	/* Set format string, if not already set */
	if (format == NULL || *format == '\0')
		if (((format = getenv("CFTIME")) == 0) || *format == 0)
			format =  "%C";

	return ((int)strftime(buf, LONG_MAX, format, tm));
}
