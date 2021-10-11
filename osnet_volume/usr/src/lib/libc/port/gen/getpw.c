/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 *	Copyright (c) 1992-1996 Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)getpw.c	1.19	97/11/24 SMI"	/* SVr4.0 1.10	*/

/*	3.0 SID #	1.2	*/
/*LINTLIBRARY*/
#pragma weak getpw = _getpw

#include "synonyms.h"
#include "file64.h"
#include <sys/types.h>
#include <mtlib.h>
#include <ctype.h>
#include <thread.h>
#include <synch.h>
#include <stdio.h>
#include "stdiom.h"
#include "libc.h"

static FILE *pwf;
#ifdef _REENTRANT
static mutex_t _pwlock = DEFAULTMUTEX;
#endif _REENTRANT
const char *PASSWD = "/etc/passwd";

int
getpw(uid_t uid, char buf[])
{
	int n, c;
	char *bp;

	if (pwf == 0) {
		(void) _mutex_lock(&_pwlock);
		if (pwf == 0) {
			pwf = fopen(PASSWD, "r");
			if (pwf == NULL) {
				(void) _mutex_unlock(&_pwlock);
				return (1);
			}
		}
		(void) _mutex_unlock(&_pwlock);
	}
	flockfile(pwf);
	_rewind_unlocked(pwf);

	for (;;) {
		bp = buf;
		while ((c = GETC(pwf)) != '\n') {
			if (c == EOF) {
				funlockfile(pwf);
				return (1);
			}
			*bp++ = (char)c;
		}
		*bp = '\0';
		bp = buf;
		n = 3;
		while (--n)
			while ((c = *bp++) != ':')
				if (c == '\n') {
					funlockfile(pwf);
					return (1);
				}
		while ((c = *bp++) != ':')
			if (isdigit(c))
				n = n*10+c-'0';
			else
				continue;
		if (n == uid) {
			funlockfile(pwf);
			return (0);
		}
	}
}
