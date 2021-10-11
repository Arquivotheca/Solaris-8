/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)output.c	1.11	93/10/18 SMI"	/* SVr4.0 1.10	*/

#include "stdio.h"
#include "errno.h"
#include "sys/types.h"

#include "lp.h"
#include "printers.h"
#include "class.h"
#include "msgs.h"

#define	WHO_AM_I	I_AM_LPSTAT
#include "oam.h"

#include "lpstat.h"

/*
 * output() - RECEIVE MESSAGE BACK FROM SPOOLER, PRODUCE OUTPUT
 */

int
output(int type)
{
	char			buffer[MSGMAX];

	int			rc;

	char			*class,
				*user,
				*reject_reason,
				*request_id,
				*printer,
				*paperAllowed,
				*form,
				*file,
				*character_set,
				*disable_reason;

	short			printer_status,
				class_status,
				state,
				status,
				rank = 0;

	long			size,
				enable_date,
				reject_date,
				date;


	if (!scheduler_active) {
		static int		complained	= 0;

		if (!complained) {
			LP_ERRMSG (ERROR, E_LP_NEEDSCHED);
			exit_rc = 1;
			complained = 1;
		}
		return (MOK);
	}

	status = MOKMORE;
	while (status == MOKMORE) {

		if ((rc = mrecv(buffer, MSGMAX)) != type) {
			if (rc == -1 && errno == EIDRM)
				LP_ERRMSG (ERROR, E_LP_MRECV);
			else
				LP_ERRMSG1 (ERROR, E_LP_BADREPLY, rc);
			done (1);
		}

		switch (type) {

		case R_PAPER_ALLOWED:
			if (getmessage(
				buffer,
				R_PAPER_ALLOWED,
				&status,
				&printer,
				&paperAllowed
			) == -1) {
				LP_ERRMSG1 (ERROR, E_LP_GETMSG, PERROR);
				done (1);
			}

			switch (status) {
			case MOK:
			case MOKMORE:
				putppline(printer, paperAllowed);
				break;

			}
			break;

		case R_INQUIRE_REQUEST:
			if (getmessage(
				buffer,
				R_INQUIRE_REQUEST,
				&status,
				&request_id,
				&user,
				&size,
				&date,
				&state,
				&printer,
				&form,
				&character_set,
				&file) == -1)
			{
				LP_ERRMSG1 (ERROR, E_LP_GETMSG, PERROR);
				done (1);
			}

			switch (status) {

			case MOK:
			case MOKMORE:
				putoline(request_id, user, size, date, state,
					printer, form, character_set, ++rank);
				break;

			}
			break;

		case R_INQUIRE_REQUEST_RANK:
			if (getmessage(
				buffer,
				R_INQUIRE_REQUEST_RANK,
				&status,
				&request_id,
				&user,
				&size,
				&date,
				&state,
				&printer,
				&form,
				&character_set,
				&rank,
				&file) == -1)
			{
				LP_ERRMSG1 (ERROR, E_LP_GETMSG, PERROR);
				done (1);
			}

			switch (status) {

			case MOK:
			case MOKMORE:
				putoline(request_id, user, size, date, state,
					printer, form, character_set, rank);
				break;

			}
			break;

		case R_INQUIRE_PRINTER_STATUS:
			if (getmessage(
				buffer,
				R_INQUIRE_PRINTER_STATUS,
				&status,
				&printer,
				&form,
				&character_set,
				&disable_reason,
				&reject_reason,
				&printer_status,
				&request_id,
				&enable_date,
				&reject_date) == -1)
			{
				LP_ERRMSG1 (ERROR, E_LP_GETMSG, PERROR);
				done (1);
			}

			switch (status) {

			case MOK:
			case MOKMORE:
				switch (inquire_type) {
				case INQ_ACCEPT:
					putqline(printer,
						(printer_status & PS_REJECTED),
						reject_date, reject_reason);
					break;

				case INQ_PRINTER:
					putpline(printer, printer_status,
						request_id, enable_date,
						disable_reason, form,
						character_set);
					break;

				case INQ_STORE:
					add_mounted (
						printer,
						form,
						character_set);
					break;

				}
				break;

			}
			break;

		case R_INQUIRE_CLASS:
			if (getmessage(
				buffer,
				R_INQUIRE_CLASS,
				&status,
				&class,
				&class_status,
				&reject_reason,
				&reject_date) == -1)
			{
				LP_ERRMSG1 (ERROR, E_LP_GETMSG,	PERROR);
				done (1);
			}

			switch (status) {

			case MOK:
			case MOKMORE:
				switch (inquire_type) {
				case INQ_ACCEPT:
					putqline(class,
						(class_status & CS_REJECTED),
						reject_date, reject_reason);
					break;
				}
				break;

			}
			break;
		}
	}

	switch (status) {

	case MOK:
	case MNOINFO:
	case MNODEST:
	case MUNKNOWN:
		return (status);

	default:	/* we are lost */
		LP_ERRMSG1 (ERROR, E_LP_BADSTATUS, status);
		done(1);

	}
	/*NOTREACHED*/
}
