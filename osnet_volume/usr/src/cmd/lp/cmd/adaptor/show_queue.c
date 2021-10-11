/*
 * Copyright (c) 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)show_queue.c	1.9	97/12/08 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <libintl.h>

#include <syslog.h>

/* lpsched include files */
#include "lp.h"
#include "msgs.h"
#include "printers.h"
#include "class.h"

#include "misc.h"

static char *
status_string(const char *printer, int *bad_status)
{
	short status, prstatus;
	char *prname = NULL,
		*form = NULL,
		*pwheel = NULL,
		*dis_reason = NULL,
		*rej_reason = NULL,
		*reqid = NULL;
	time_t *dis_date = NULL,
		 *rej_date = NULL;
	static char mesg[BUFSIZ];

	*bad_status = 1; /* assume it's bad news */

	if (printer == NULL)
		return ("no destination");

	if (isprinter((char *)printer)) {
		if ((snd_msg(S_INQUIRE_PRINTER_STATUS, printer) < 0) ||
		    (rcv_msg(R_INQUIRE_PRINTER_STATUS, &status, &prname, &form,
				&pwheel, &dis_reason, &rej_reason, &prstatus,
				&reqid, &dis_date, &rej_date) < 0))
			status = MTRANSMITERR;
	} else if (isclass(printer)) {
		if ((snd_msg(S_INQUIRE_CLASS, printer) < 0) ||
		    (rcv_msg(R_INQUIRE_CLASS, &status, &prname,
				&prstatus, &rej_reason, &rej_date) < 0))
			status = MTRANSMITERR;
	} else {
		snprintf(mesg, sizeof (mesg), 
			gettext("%s: not a printer or class"), printer);
		return (mesg);
	}

	switch (status) {
	case MNODEST:
		snprintf(mesg, sizeof (mesg),
			gettext("unknown destination: %s"), printer);
		break;
	case MNOINFO:
		snprintf(mesg, sizeof (mesg),
			gettext("unknown status: %s"), printer);
		break;
	case MTRANSMITERR:
		sprintf(mesg, gettext("failure to communicate with lpsched"));
		break;
	case MOK:
		if (prstatus & (PS_DISABLED | PS_FAULTED))
			sprintf(mesg, gettext("Warning: %s is down: %s\n"),
				prname, dis_reason);
		else if (prstatus & PS_REJECTED)
			sprintf(mesg,
			    gettext("Warning: %s queue is turned off: %s\n"),
			    prname, rej_reason);
		else if (!(prstatus & (PS_DISABLED | PS_FAULTED))) {
			*bad_status = 0;
			sprintf(mesg, gettext("%s is ready and printing\n"),
				prname);
		}

		break;
	default:
		snprintf(mesg, sizeof (mesg),
			gettext("bad status: %s, 0x%x"), printer, status);
		break;
	}

	return (mesg);
}


static char *_rank_suffixes[] = {
	"th", "st", "nd", "rd", "th", "th", "th", "th", "th", "th"
};


static char *
rank_string(const int rank)
{
	static char buf[12];

	if (rank < 0)
		sprintf(buf, gettext("invalid"));
	else if (rank == 0)
		sprintf(buf, gettext("active"));
	else if ((rank > 10) && (rank < 14))
		sprintf(buf, "%dth", rank);
	else
		sprintf(buf, "%d%s", rank, _rank_suffixes[rank % 10]);

	return (buf);
}


static int
is_matched(int id, char *user, const char **list)
{
	if ((list == NULL) || (list[0] == NULL))
		return (1);

	while (*list != NULL)
		if ((strcmp(user, *list) == 0) || (atoi(*list) == id))
			return (1);
		else
			list++;
	return (0);
}


#define	HEADER gettext("Rank\tOwner\tJob\tFile(s)\t\t\t\tTotal Size\n")

static int
job_list(const char *printer, FILE *ofp, const int type, const char **list,
		const char *status_message, int *rank)
{
	int count = 0;
	short status, outcome;


	if (snd_msg(S_INQUIRE_REQUEST, "", printer, "", "", "") < 0)
		return (0);
	do {
		size_t size;
		time_t date;
		int id;
		char *user, *reqid, *owner, *dest, *form, *pwheel, *file, *host;

		if (rcv_msg(R_INQUIRE_REQUEST, &status, &reqid, &owner, &size,
				&date, &outcome, &dest, &form, &pwheel,
				&file) < 0)
			return (count);

		host = (char *)user_host(owner);
		user = (char *)user_name(owner);
		id = id_no(reqid);

		if (is_matched(id, user, list) == 0)
			continue;

		switch (status) {
		case MOK:
		case MOKMORE:
			count++;
			if (status_message != NULL) {
				fprintf(ofp, "%s", status_message);
				if (type == 3)
					fprintf(ofp, HEADER);
				status_message = NULL;
			}
			if (type == 3) {	/* short format */
				fprintf(ofp,
				gettext("%s\t%s\t%d\t%-32.32s%d bytes\n"),
					rank_string((*rank)++), user,
					id, file, size);
			} else {		/* long format */
				fprintf(ofp,
		gettext("\n%s: %s\t\t\t\t[job %d %s]\n\t%-32.32s\t%d bytes\n"),
					user, rank_string((*rank)++),
					id, host, file, size);
			}

		}
	} while (status == MOKMORE);

	return (count);
}

/*
 * lpsched_show_queue() attempts to display the queue of "pending" jobs.  The
 * "type" is used to determine if this should be a short or long format
 * that gets written back to ofp.
 */
int
lpsched_show_queue(const char *printer, FILE *ofp, const int type,
			const char **list)
{
	char *status_message = NULL;
	int rank = 0;
	char **plist = NULL;
	CLASS	*clp = NULL;

	syslog(LOG_DEBUG, "lpsched_show_queue(%s, %d, %d, 0x%x)",
		(printer ? printer : "NULL"), ofp, type, list);

	if ((printer == NULL) || (list == NULL))
		return (-1);

	status_message = status_string(printer, &rank);

	if (isclass((char *)printer) &&
	    ((clp = getclass((char *)printer)) != NULL))
		plist = clp->members;

	do {
		if (plist != NULL)
			printer = *(plist++);

		if (job_list(printer, ofp, type, list,
				(const char *)status_message, &rank) > 0)
			status_message = NULL;

	} while ((plist != NULL) && (*plist != NULL));

	if (rank == 0)
		fprintf(ofp, gettext("no entries\n"));
	else if (status_message != NULL)
		fprintf(ofp, "%s\n", status_message);

	return (0);
}
