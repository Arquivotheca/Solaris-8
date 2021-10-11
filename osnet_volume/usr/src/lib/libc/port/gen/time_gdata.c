/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)time_gdata.c	1.12	97/03/10 SMI"

/*LINTLIBRARY*/

#pragma weak altzone = _altzone
#pragma weak daylight = _daylight
#pragma weak timezone = _timezone
#pragma weak tzname = _tzname

#include	"synonyms.h"
#include 	<mtlib.h>
#include	<sys/types.h>
#include 	<time.h>
#include	<synch.h>

long int	timezone = 0;	/* XPG4 version 2 */
long int	altzone = 0;	/* follow suit */
int 		daylight = 0;
extern char	_tz_gmt[];
extern char	_tz_spaces[];
char 		*tzname[] = {_tz_gmt, _tz_spaces};
#ifdef _REENTRANT
mutex_t _time_lock = DEFAULTMUTEX;
#endif _REENTRANT
