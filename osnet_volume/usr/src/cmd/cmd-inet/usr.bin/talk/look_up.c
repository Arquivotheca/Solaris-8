/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)look_up.c 1.6	94/10/04 SMI"	/* SVr4.0 1.3   */


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
#include <libintl.h>
#include <sys/isa_defs.h>

#ifdef SYSV
#define	bcopy(a, b, c)	memcpy((b), (a), (c))
#endif /* SYSV */

static int look_for_invite(CTL_RESPONSE *);
static CTL_RESPONSE swapresponse();


	/* see if the local daemon has a invitation for us */

int
check_local()
{
	CTL_RESPONSE response;

	/* the rest of msg was set up in get_names */

	msg.ctl_addr = ctl_addr;

	if (!look_for_invite(&response)) {

		/* we must be initiating a talk */

		return (0);
	}

	/*
	 * there was an invitation waiting for us,
	 * so connect with the other (hopefully waiting) party
	 */

	current_state = gettext("Waiting to connect with caller");

	response = swapresponse(response);
	while (connect(sockt, (struct sockaddr *)&response.addr,
		sizeof (response.addr)) != 0) {
		if (errno == ECONNREFUSED) {

			/*
			 * the caller gave up, but the invitation somehow
			 * was not cleared. Clear it and initiate an
			 * invitation. (We know there are no newer invitations,
			 * the talkd works LIFO.)
			 */

			ctl_transact(rem_machine_addr, msg, DELETE, &response);
			close(sockt);
			open_sockt();
			return (0);
		} else if (errno == EINTR) {

		/* we have returned from an interupt handler */
			continue;
		} else {
			p_error(gettext("Unable to connect with initiator"));
		}
	}

	return (1);
}

	/* look for an invitation on 'machine' */

static int
look_for_invite(response)
CTL_RESPONSE *response;
{
	current_state = gettext("Checking for invitation on caller's machine");

	ctl_transact(rem_machine_addr, msg, LOOK_UP, response);

	/*
	 * switch is for later options, such as multiple invitations
	*/

	switch (response->answer) {

	case SUCCESS:

		msg.id_num = response->id_num;
		return (1);

	default :
		/* there wasn't an invitation waiting for us */
		return (0);
	}
}

/*
 * heuristic to detect if need to reshuffle CTL_RESPONSE structure
 */

#if defined(_LITTLE_ENDIAN)
struct ctl_response_runrise {
	char type;
	char answer;
	short junk;
	int id_num;
	struct sockaddr_in addr;
};

static CTL_RESPONSE
swapresponse(rsp)
	CTL_RESPONSE rsp;
{
	struct ctl_response_runrise swaprsp;

	if (rsp.addr.sin_family != AF_INET) {
		bcopy(&rsp, &swaprsp, sizeof (CTL_RESPONSE));
		if (swaprsp.addr.sin_family == AF_INET) {
			rsp.addr = swaprsp.addr;
			rsp.type = swaprsp.type;
			rsp.answer = swaprsp.answer;
			rsp.id_num = swaprsp.id_num;
		}
	}
	return (rsp);
}
#endif

#if defined(_BIG_ENDIAN)
struct ctl_response_sun3 {
	char type;
	char answer;
	unsigned short id_num2;
	unsigned short id_num1;
	short sin_family;
	short sin_port;
	short sin_addr2;
	short sin_addr1;
};

static CTL_RESPONSE
swapresponse(rsp)
	CTL_RESPONSE rsp;
{
	struct ctl_response_sun3 swaprsp;

	if (rsp.addr.sin_family != AF_INET) {
		bcopy(&rsp, &swaprsp, sizeof (struct ctl_response_sun3));
		if (swaprsp.sin_family == AF_INET) {
			rsp.type = swaprsp.type;
			rsp.answer = swaprsp.answer;
			rsp.id_num = swaprsp.id_num1
				| (swaprsp.id_num2 << 16);
			rsp.addr.sin_family = swaprsp.sin_family;
			rsp.addr.sin_port = swaprsp.sin_port;
			rsp.addr.sin_addr.s_addr =
				(swaprsp.sin_addr2 << 16)| swaprsp.sin_addr1;
		}
	}
	return (rsp);
}
#endif
