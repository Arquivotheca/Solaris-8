/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* 	Portions Copyright(c) 1988, Sun Microsystems Inc.	*/
/*	All Rights Reserved					*/

#pragma ident	"@(#)gettimeofday.c	1.9	97/06/16 SMI"	/* SVr4.0 1.1	*/

/*LINTLIBRARY*/

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/syscall.h>
#include "libc.h"

/*
 * Get the time of day information.
 * BSD compatibility on top of SVr4 facilities:
 * u_sec always zero, and don't do anything with timezone pointer.
 */

/*
 * This is defined in sys/gettimeofday.s
 * This extern cannot be in libc.h due to name conflict with port/gen/synonyms.h
 */
extern _gettimeofday(struct timeval *);

/*ARGSUSED*/
int
gettimeofday(struct timeval *tp, void *tzp)
{
	if (tp == NULL)
		return (0);

	return (_gettimeofday(tp));
}

/*
 * Set the time.
 * Don't do anything with the timezone information.
 */

/*ARGSUSED*/
int
settimeofday(struct timeval *tp, void *tzp)
{
	time_t t;		/* time in seconds */

	if (tp == NULL)
		return (0);

	t = (time_t) tp->tv_sec;
	if (tp->tv_usec >= 500000)
		/* round up */
		t++;

	return (stime(&t));
}
