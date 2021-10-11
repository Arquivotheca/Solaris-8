/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)usg.local.c	1.13	98/08/06 SMI"
		/* from SVr4.0 1.2.2.3 */

/*
 * mailx -- a modified version of a University of California at Berkeley
 *	mail program
 *
 * Local routines that are installation dependent.
 */

#include "rcv.h"

static	int ismailbox(char *file);

/*
 * Locate the user's mailbox file (ie, the place where new, unread
 * mail is queued).  In SVr4 UNIX, it is in /var/mail/name.
 * In preSVr4 UNIX, it is in either /usr/mail/name or /usr/spool/mail/name.
 */
void 
findmail(char *name)
{
	register char *cp;

	if (name != NOSTR) {
		copy(name, copy(maildir, mailname));
		issysmbox = 1;	/* it's a system mailbox */
	} else if ((cp = getenv("MAIL")) != NULL) {
		/* if $MAIL is set, use it */
		nstrcpy(mailname, PATHSIZE, cp);
		issysmbox = ismailbox(mailname);
		/* XXX - should warn that there's no locking? */
	} else {
		copy(myname, copy(maildir, mailname));
		issysmbox = 1;
	}
	if (issysmbox)
		lockname = strrchr(mailname, '/') + 1;
}

/*
 * Make sure file matches (/usr|/var)(/spool)?/mail/.
 * If is does, it's a "system mailbox", return true.
 */
static int
ismailbox(char *file)
{
#ifdef preSVr4
	return (strncmp(file, maildir, strlen(maildir)) == 0);
#else
	if (strncmp(file, "/var", 4) != 0
	    && strncmp(file, "/usr", 4) != 0
	    )
		return (0);
	file += 4;
	if (strncmp(file, "/spool", 6) == 0)
		file += 6;
	return (strncmp(file, "/mail/", 6) == 0);
#endif
}
