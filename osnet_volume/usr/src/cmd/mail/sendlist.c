

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)sendlist.c	1.12	94/04/06 SMI"	/* SVr4.0 1.4 */

#include "mail.h"
/*
    NAME
	sendlist - send copy to specified users

    SYNOPSIS
	int sendlist(reciplist *list, int letnum, int level)

    DESCRIPTION
	sendlist() will traverse the current recipient list and
	send a copy of the given letter to each user specified,
	invoking send() to do the sending. It returns
	1 if the sending fails, 0 otherwise.
 */


/*
 * mailx and mailtool read the SENDMAIL from an environment, since few
 *  people use /bin/mail as their user agent and since /bin/mail is often
 *  called as root or made setuid it's safer to leave this hardwired.
 */

static char *sendmail_prog = SENDMAIL;

int
sendlist(list, letnum, level)
reciplist	*list;
int		letnum;
int		level;
{
	recip *to;
	char *cmd;
	int cmd_len = 0;
	int rc = 0;
	FILE *fp;

	/* Deliver mail directly to a mailbox */
	if (deliverflag) {
		/*
		 * Note failure to deliver to any one of the recipients
		 * should be considered a failure, so that the user
		 * get's an indication of that failure.
		 */
		for (to = &(list->recip_list); to; to = to->next) {
			if (to->name)
				if (!send_mbox(to->name, letnum))
					rc = 1;
		}
		return (rc);
	}

	/*
	 * build argv list, allowing for arbitrarily long deliver lists
	 * and then  hand the message off to sendmail
	 */

	if (!ismail)   /* We're rmail and we need to do sendmail -fRpath */
		cmd_len += strlen(Rpath) + strlen(" -f") + 2;

	/* Add 1 so we have room for a space after each recipient name */
	for (to = &(list->recip_list); to; to = to->next)
		if (to->name)
			cmd_len += (1 + strlen(to->name));

	/* plus 4 for the added " -oi" */
	cmd_len += (1 + strlen(sendmail_prog) + 4);
	cmd = (char *)malloc(cmd_len * sizeof (char));

	/* Copy the name of the mailer */
	(void) strcpy(cmd, sendmail_prog);

	/* If we're rmail copy -fRpath to the the command line */
	if (!ismail) {
		(void) strcat(cmd, " -f");
		(void) strcat(cmd, "\'");
		(void) strcat(cmd, Rpath);
		(void) strcat(cmd, "\'");
	}

	(void) strcat(cmd, " -oi");

	for (to = &(list->recip_list); to; to = to->next)
		if (to->name) {
			(void) strcat(cmd, " ");
			strcat(cmd, to->name);
		}

	fp = popen(cmd, "w");
	copylet(letnum, fp, ORDINARY);
	rc = pclose(fp);
	free(cmd);
	if (!rc)
		return (0);
	else
		return (1);
}

/*
 * send_mbox(user, letnum)  Sends the letter specified by letnum to the
 *	"user"'s mailbox. It returns 1 if the sending fails;
 *	0 on success.
 */



int
send_mbox(mbox, letnum)
char	*mbox;
int	letnum;
{
	char file[PATH_MAX];
	char biffmsg[PATH_MAX];
	int mbfd;
	FILE *malf;
	int rc;
	uid_t useruid, saved_uid;
	void (*istat)(), (*qstat)(), (*hstat)();

	if (!islocal(mbox, &useruid))
		return (1);
	strncpy(file, maildir, sizeof (file));
	strncat(file, mbox, sizeof (file) - strlen(mbox));

	/*
	 * We need to setgid and seteuid here since the users's mail box
	 * might be NFS mounted and since root can't write across NFS.
	 * Note this won't work with Secure NFS/RPC's.  Since delivering to
	 * NFS mounted directories isn't really supported that's OK for now.
	 */
	setgid(mailgrp);
	saved_uid = geteuid();
	seteuid(useruid);
	lock(mbox);

	/* ignore signals */
	istat = signal(SIGINT, SIG_IGN);
	qstat = signal(SIGQUIT, SIG_IGN);
	hstat = signal(SIGHUP, SIG_IGN);
	/* now access mail box */
	mbfd = accessmf(file);
	if (mbfd == -1) {	/* mail box access failed, bail out */
		unlock();
		rc = FALSE;
		sav_errno = EACCES;
		goto done;
	} else {
				/* mail box is ok, now do append */
		if ((malf = fdopen(mbfd, "a")) != NULL) {
			sprintf(biffmsg, "%s@%d\n", mbox, ftell(malf));
			rc = copylet(letnum, malf, ORDINARY);
			fclose(malf);
		}
	}

	if (rc == FALSE)
		fprintf(stderr, "%s: Cannot append to %s\n", program, file);
	else
		notifybiff(biffmsg);

done:
	/* restore signal */
        (void) signal(SIGINT, istat);
        (void) signal(SIGQUIT, qstat);
        (void) signal(SIGHUP, hstat);
	unlock();
	seteuid(saved_uid);
	return (rc);
}

#include <sys/socket.h>
#include <netinet/in.h>

notifybiff(msg)
	char *msg;
{
	static struct sockaddr_in addr;
	static int f = -1;

	if (addr.sin_family == 0) {
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_LOOPBACK;
		addr.sin_port = htons(IPPORT_BIFFUDP);
	}
	if (f < 0)
		f = socket(AF_INET, SOCK_DGRAM, 0);
	sendto(f, msg, strlen(msg)+1, 0, (struct sockaddr *) &addr,
		sizeof (addr));
}
