/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getpass.c	1.17	99/03/02 SMI"	/* SVr4.0 1.20	*/

/*LINTLIBRARY*/

#pragma weak getpass = _getpass
#pragma weak getpassphrase = _getpassphrase

#include "synonyms.h"
#include "file64.h"
#include "mtlib.h"
#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <stropts.h>
#include <termio.h>
#include <thread.h>
#include <synch.h>
#include "libc.h"
#include "stdiom.h"
#include "tsd.h"

static void catch(int);
static int intrupt;
static char *__getpass(const char *, int);

#define	MAXPASSWD	256	/* max significant characters in password */
#define	SMLPASSWD	8	/* unix standard  characters in password */


char *
getpass(const char *prompt)
{
	return ((char *)__getpass(prompt, SMLPASSWD));
}

char *
getpassphrase(const char *prompt)
{
	return ((char *)__getpass(prompt, MAXPASSWD));
}

static char *
__getpass(const char *prompt, int size)
{
	struct termio ttyb;
	unsigned short flags;
	char *p;
	int c;
	FILE	*fi;
	static char pbuf_st[MAXPASSWD + 1];
	char *pbuf = (_thr_main() ? pbuf_st :
		(char *)_tsdbufalloc(_T_GETPASS,
		    (size_t)1, sizeof (char) * (MAXPASSWD + 1)));
	void	(*sig)(int);
#ifdef _REENTRANT
	rmutex_t *lk;
#endif _REENTRANT

	if ((fi = fopen("/dev/tty", "r")) == NULL)
		return ((char *)NULL);
	setbuf(fi, (char *)NULL);
	sig = signal(SIGINT, catch);
	intrupt = 0;
	(void) ioctl(FILENO(fi), TCGETA, &ttyb);
	flags = ttyb.c_lflag;
	ttyb.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
	(void) ioctl(FILENO(fi), TCSETAF, &ttyb);
	FLOCKFILE(lk, stderr);
	(void) fputs(prompt, stderr);
	p = pbuf;
	while (!intrupt &&
		(c = GETC(fi)) != '\n' && c != '\r' && c != EOF) {
		if (p < &pbuf[ size ])
			*p++ = (char)c;
	}
	*p = '\0';
	ttyb.c_lflag = flags;
	(void) ioctl(FILENO(fi), TCSETAW, &ttyb);
	(void) PUTC('\n', stderr);
	FUNLOCKFILE(lk);
	(void) signal(SIGINT, sig);
	(void) fclose(fi);
	if (intrupt)
		(void) kill(getpid(), SIGINT);
	return (pbuf);
}

static void
catch(int x)
{
	intrupt = 1;
}
