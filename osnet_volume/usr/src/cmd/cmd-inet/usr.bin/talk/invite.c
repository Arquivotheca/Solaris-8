/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)invite.c	1.4	94/10/04 SMI"	/* SVr4.0 1.3	*/

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
 * 	(c) 1986,1987,1988.1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */


#include "talk_ctl.h"
#include <sys/time.h>
#include <signal.h>
#include <setjmp.h>
#include <libintl.h>

#ifdef SYSV
#define	signal(s, f)	sigset(s, f)
#endif /* SYSV */

	/*
	* there wasn't an invitation waiting, so send a request containing
	* our sockt address to the remote talk daemon so it can invite
	* him
	*/

static int local_id, remote_id;

	/*
	* the msg.id's for the invitations
	* on the local and remote machines.
	* These are used to delete the invitations.
	*/

static jmp_buf	invitebuf;

static void re_invite();
static void announce_invite();

void
invite_remote()
{
	int new_sockt;
	struct itimerval itimer;
	CTL_RESPONSE response;

	itimer.it_value.tv_sec = RING_WAIT;
	itimer.it_value.tv_usec = 0;
	itimer.it_interval = itimer.it_value;

	if (listen(sockt, 5) != 0) {
		p_error(gettext("Error on attempt to listen for caller"));
	}

	msg.addr = my_addr;
	msg.id_num = -1;		/* an impossible id_num */

	invitation_waiting = 1;

	announce_invite();

	/*
	 * shut off the automatic messages for a while,
	 * so we can use the interupt timer to resend the invitation
	 */

	end_msgs();
	setitimer(ITIMER_REAL, &itimer, (struct itimerval *)0);
	message(gettext("Waiting for your party to respond"));
	signal(SIGALRM, re_invite);
	(void) setjmp(invitebuf);

	while ((new_sockt = accept(sockt, 0, 0)) < 0) {
		if (errno != EINTR) {
			p_error(gettext("Unable to connect with your party"));
		} else {
		/* we just returned from a interupt, keep trying */
			continue;
		}
	}

	close(sockt);
	sockt = new_sockt;

	/*
	* have the daemons delete the invitations now that we have connected.
	*/

	current_state = strdup(gettext("Waiting for your party to respond"));
	start_msgs();

	msg.id_num = local_id;
	ctl_transact(my_machine_addr, msg, DELETE, &response);
	msg.id_num = remote_id;
	ctl_transact(rem_machine_addr, msg, DELETE, &response);
	invitation_waiting = 0;
}

	/* routine called on interupt to re-invite the callee */

static void
re_invite()
{
	message(gettext("Ringing your party again"));
	current_line++;
	/* force a re-announce */
	msg.id_num = remote_id + 1;
	announce_invite();
	longjmp(invitebuf, 1);
}

	/* transmit the invitation and process the response */

static void
announce_invite()
{
	CTL_RESPONSE response;

	current_state =
		gettext("Trying to connect to your party's talk daemon");

	ctl_transact(rem_machine_addr, msg, ANNOUNCE, &response);
	remote_id = response.id_num;

	if (response.answer != SUCCESS) {

		switch (response.answer) {

			case NOT_HERE :
			message(gettext("Your party is not logged on"));
			break;

			case MACHINE_UNKNOWN :
			message(
			gettext("Target machine does not recognize us"));
			break;

			case UNKNOWN_REQUEST :
			message(
			gettext("Target machine can not handle remote talk"));
			break;

			case FAILED :
			message(
		gettext("Target machine is too confused to talk to us"));
			break;

			case PERMISSION_DENIED :
			message(gettext("Your party is refusing messages"));
			break;
		}

		quit();
	}

	/* leave the actual invitation on my talk daemon */

	ctl_transact(my_machine_addr, msg, LEAVE_INVITE, &response);
	local_id = response.id_num;
}

void
send_delete()
{

	/* tell the daemon to remove your invitation */

	msg.type = DELETE;

	/*
	* this is just a extra clean up, so just send it
	* and don't wait for an answer
	*/

	msg.id_num = remote_id;
	daemon_addr.sin_addr = rem_machine_addr;
	if (sendto(ctl_sockt, (char *)&msg, sizeof (CTL_MSG), 0,
		(struct sockaddr *)&daemon_addr,
			sizeof (daemon_addr)) != sizeof (CTL_MSG)) {
		perror(gettext("send_delete remote"));
	}

	msg.id_num = local_id;
	daemon_addr.sin_addr = my_machine_addr;
	if (sendto(ctl_sockt, (char *)&msg, sizeof (CTL_MSG), 0,
		(struct sockaddr *)&daemon_addr,
			sizeof (daemon_addr)) != sizeof (CTL_MSG)) {
		perror(gettext("send_delete local"));
	}
}
