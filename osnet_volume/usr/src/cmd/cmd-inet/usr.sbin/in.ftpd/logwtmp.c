/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)logwtmp.c	1.16	99/09/16 SMI"	/* SVr4.0 1.2 */

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	Copyright (c) 1986-1999 by Sun Microsystems, Inc.
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	All rights reserved.
 *
 */


#include <sys/types.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <utmpx.h>
#include <sac.h>	/* for SC_WILDC */
#include <fcntl.h>
#include <unistd.h>

#include <syslog.h>

static int fd;

unsigned char ut_id[4] = { 'f', 't', 'p', (unsigned char) SC_WILDC };

/*
 * Since logwtmp is only called in two places, on log in and on logout
 * and since on logout the 2nd parameter is NULL we check for that to
 * to determine if we are starting or ending a session. Rather gross,
 * but it allows us to avoid changing a bunch of other code.
 */


void
logwtmp(line, name, host)
	char *line, *name, *host;
{
	int uid, error;

	/* next 3 variables needed for utmp management */
	int			tmplen;
	struct utmpx		set_utmp;
	char			*ttyntail;

	/*  the following 2 variables are needed for utmp mgmt */
	struct utmpx		ut;
	int			reset_pid;

	if (name == NULL)
		return;

	uid = geteuid();
	seteuid(0);

	/* clear wtmpx entry */
	(void) memset((char *)&ut, 0, sizeof (ut));

	/* fill in wtmpx fields */
	(void) strncpy(ut.ut_user, name, sizeof (ut.ut_user));
	(void) memcpy(ut.ut_id, ut_id, sizeof (ut.ut_id));
	(void) strncpy(ut.ut_line, line, sizeof (ut.ut_line));
	ut.ut_pid  = getpid();
	ut.ut_type = USER_PROCESS;
	ut.ut_exit.e_termination = 0;
	ut.ut_exit.e_exit = 0;
	ut.ut_syslen = strlen(host)+1;
	(void) strncpy(ut.ut_host, host, sizeof (ut.ut_host));
	(void) gettimeofday(&ut.ut_tv, NULL);
	updwtmpx(WTMPX_FILE, &ut);
	seteuid(uid);
}
