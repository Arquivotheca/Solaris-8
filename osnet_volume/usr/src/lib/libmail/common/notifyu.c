/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)notifyu.c	1.7	99/03/09 SMI" 	/* SVr4.0 1.4	*/
/*LINTLIBRARY*/

#include "synonyms.h"
#include "libmail.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utmpx.h>
#include <syslog.h>
#if !defined(__cplusplus) && !defined(c_plusplus)
typedef void (*SIG_PF) (int);
#endif
#include <unistd.h>
#include <signal.h>

static SIG_PF catcher(void);

static SIG_PF catcher(void)
{
	/* do nothing, but allow the write() to break */
	return (0);
}

void
notify(char *user, char *msg, int check_mesg_y, char *etcdir)
{
	/* search the utmp file for this user */
	SIG_PF old;
	unsigned int oldalarm;
	struct utmpx utmpx, *putmpx = &utmpx;

	setutxent();

	/* grab the tty name */
	while ((putmpx = getutxent()) != NULL) {
		if (strncmp(user, utmpx.ut_name,
		    sizeof (utmpx.ut_name)) == 0) {
			char tty[sizeof (utmpx.ut_line)+1];
			char dev[MAXFILENAME];
			FILE *port;
			size_t i;
			int fd;

			for (i = 0; i < sizeof (utmpx.ut_line); i++)
				tty[i] = utmpx.ut_line[i];
			tty[i] = '\0';

			/* stick /dev/ in front */
			(void) sprintf(dev, "%s/dev/%s", etcdir, tty);

			/* break out if write() to the tty hangs */
			old = (SIG_PF)signal(SIGALRM, (SIG_PF)catcher);
			oldalarm = alarm(300);

			/* check if device is really a tty */
			if ((fd = open(dev, O_WRONLY|O_NOCTTY)) == -1) {
				(void) fprintf(stderr,
				    "Cannot open %s.\n", dev);
				continue;
			} else {
				if (!isatty(fd)) {
					(void) fprintf(stderr, "%s in utmpx is "
					    "not a tty\n", tty);
					openlog("mail", 0, LOG_AUTH);
					syslog(LOG_CRIT, "%s in utmp is "
					    "not a tty\n", tty);
					closelog();
					(void) close(fd);
					continue;
				}
			}
			(void) close(fd);

			/* write to the tty */
			port = fopen(dev, "w");
			if (port != 0) {
				(void) fprintf(port, "\r\n%s\r\n", msg);
				(void) fclose(port);
			}

			/* clean up our alarm */
			(void) alarm(0);
			(void) signal(SIGALRM, old);
			(void) alarm(oldalarm);
		}
	}
	endutxent();
}
