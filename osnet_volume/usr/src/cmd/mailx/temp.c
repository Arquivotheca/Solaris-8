/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)temp.c	1.17	98/09/10 SMI"	/* from SVr4.0 1.5.2.3 */

#include "rcv.h"
#include <pwd.h>
#include <locale.h>

#ifdef preSVr4
extern struct passwd *getpwnam();
extern struct passwd *getpwuid();
#endif

/*
 * mailx -- a modified version of a University of California at Berkeley
 *	mail program
 *
 * Give names to all the temporary files that we will need.
 */

void 
tinit(void)
{
	register pid_t pid = mypid;
	struct passwd *pwd;


	snprintf(tempMail, TMPSIZ, "/tmp/Rs%-ld", pid);
	snprintf(tempQuit, TMPSIZ, "/tmp/Rm%-ld", pid);
	snprintf(tempEdit, TMPSIZ, "/tmp/Re%-ld", pid);
	snprintf(tempMesg, TMPSIZ, "/tmp/Rx%-ld", pid);
	snprintf(tempZedit, TMPSIZ, "/tmp/Rz%-ld", pid);

	/* get the name associated with this uid */
	pwd = getpwuid(uid = myruid);
	if (!pwd) {
		printf(gettext("Error looking up username for uid=%d\n"), uid);
		exit(1);
	}
	else
		copy(pwd->pw_name, myname);
	endpwent();

	nstrcpy(homedir, PATHSIZE, Getf("HOME"));
	findmail(NULL);
	assign("MBOX", Getf("MBOX"));
	assign("MAILRC", Getf("MAILRC"));
	assign("DEAD", Getf("DEAD"));
	assign("save", "");
	assign("asksub", "");
	assign("header", "");
	assign("prompt", "? ");
	assign("pipeignore", "");
	assign("replyall", "");
	assign("from", "");
}
