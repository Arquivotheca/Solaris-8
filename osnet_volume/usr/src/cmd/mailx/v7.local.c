#ifndef lint
static	char *sccsid = "@(#)v7.local.c 1.12 94/03/04 SMI"; /* from UCB 2.2 07/28/82 */
#endif

/*
 * Mail -- a mail program
 *
 * Version 7
 *
 * Local routines that are installation dependent.
 */

#include "rcv.h"

/*
 * Locate the user's mailbox file (ie, the place where new, unread
 * mail is queued).  In Version 7, it is in /usr/spool/mail/name.
 */

void 
findmail(char *name)
{
	register char *cp;

	cp = copy("/usr/spool/mail/", mailname);
	copy(name != NULL ? name : myname, cp);
	issysmbox = 1;

	/*
	 * If we're looking at our own mailbox,
	 * use MAIL environment variable to locate it.
	 */
	if (name == NULL &&
	    (cp = getenv("USER")) != NULL && strcmp(cp, myname) == 0) {
		if ((cp = getenv("MAIL")) && access(cp, 4) == 0) {
			if (strcmp(cp, mailname) != 0)
				issysmbox = 0;
			copy(cp, mailname);
		}
	}
	if (issysmbox)
		lockname = strrchr(mailname, '/') + 1;
}
