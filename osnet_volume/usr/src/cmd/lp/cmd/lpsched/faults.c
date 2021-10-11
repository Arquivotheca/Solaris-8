/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)faults.c	1.9	98/08/05 SMI"	/* SVr4.0 1.2.1.4	*/
/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "lpsched.h"
#include <syslog.h>

static char *
shortenReason(char *reason)
{
	register char	*ptr, *pe;
	int		peLen;

	if (strncmp(reason,"%%[",3) == 0)
		reason += 3;

	while (*reason == ' ')
		reason++;

	pe = "PrinterError:";
	peLen = strlen(pe);
	if (strncmp(reason,pe,peLen) == 0)
		reason += peLen;

	if (((ptr = strchr(reason,']')) != NULL) && (strncmp(ptr,"]%%",3) == 0))
		*ptr = 0;

	pe = reason + strlen(reason) -1;
	pe = reason;
	while (pe = strchr(pe,'\n'))
		*pe = ' ';

	pe = reason + strlen(reason) -1;
	while ((pe > reason) && (*pe == ' ')) {
		*pe = 0;
		pe--;
	}
	return(reason);
}

/**
 ** printer_fault() - RECOGNIZE PRINTER FAULT
 **/

void
printer_fault(register PSTATUS *pps, register RSTATUS *prs, char *alert_text,
	 int err)
{
	register char		*why,*shortWhy;

	pps->status |= PS_FAULTED;

	/*  -F wait  */
	if (STREQU(pps->printer->fault_rec, NAME_WAIT))
		disable (pps, CUZ_FAULT, DISABLE_STOP);

	/*  -F beginning  */
	else if (STREQU(pps->printer->fault_rec, NAME_BEGINNING))
		terminate (pps->exec);

	/*  -F continue  AND  the interface program died  */
	else if (!(pps->status & PS_LATER) && !pps->request) {
		load_str (&pps->dis_reason, CUZ_STOPPED);
		schedule (EV_LATER, WHEN_PRINTER, EV_ENABLE, pps);
	}

	if (err) {
		errno = err;
		why = makestr(alert_text, "(", PERROR, ")\n", (char *)0);
	} else if (! alert_text)
		why = makestr("exec exit fault", (char *) 0);
	else
		why = makestr(alert_text, (char *) 0);

	if (!why)
		why = alert_text;

	shortWhy = (why != alert_text ? shortenReason(why) : why);

	load_str (&pps->fault_reason, shortWhy);
	dump_fault_status (pps);
	if (STREQU(pps->printer->fault_alert.shcmd,"show fault"))
		pps->status |= PS_SHOW_FAULT;
	else
		pps->status &= ~PS_SHOW_FAULT;

	note("printer fault. type: %s, status: %x\nmsg: (%s)\n",
		(pps->printer->fault_alert.shcmd ?
		    pps->printer->fault_alert.shcmd : "??"),
		pps->status, shortWhy);
	syslog(LOG_INFO, "fault (%s), type (%s), status(%x): %s",
		(pps->printer->name ? pps->printer->name : "NULL"),
		(pps->printer->fault_alert.shcmd ?
			pps->printer->fault_alert.shcmd : "NULL"),
		pps->status, (shortWhy ? shortWhy : "NULL"));

	if (pps->status & PS_SHOW_FAULT)
		schedule (EV_MESSAGE, pps);
	else {
		alert(A_PRINTER, pps, prs, shortWhy); 
	}
	if (why != alert_text)
		Free (why);
}

/**
 ** clear_printer_fault() - RECOGNIZE PRINTER FAULT
 **/

void
clear_printer_fault(register PSTATUS *pps, char *alert_text)
{
	register char	*why, *shortWhy;

	pps->status &= ~PS_FAULTED;

	why = makestr(alert_text, (char *) 0);

	shortWhy = (why ? shortenReason(why) : alert_text);

	load_str (&pps->fault_reason, shortWhy);
	dump_fault_status (pps);
	if (STREQU(pps->printer->fault_alert.shcmd,"show fault"))
		pps->status |= PS_SHOW_FAULT;
	else
		pps->status &= ~PS_SHOW_FAULT;

	if (pps->status & PS_SHOW_FAULT)
		schedule (EV_MESSAGE, pps);
	if (why != alert_text)
		Free(why);
	schedule(EV_ENABLE, pps);
}

/**
 ** dial_problem() - ADDRESS DIAL-OUT PROBLEM
 **/

void
dial_problem(register PSTATUS *pps, RSTATUS *prs, int rc)
{
	static struct problem {
		char			*reason;
		int			retry_max,
					dial_error;
	}			problems[] = {
		"DIAL FAILED",			10,	 2, /* D_HUNG  */
		"CALLER SCRIPT FAILED",		10,	 3, /* NO_ANS  */
		"CAN'T ACCESS DEVICE",		 0,	 6, /* L_PROB  */
		"DEVICE LOCKED",		20,	 8, /* DV_NT_A */
		"NO DEVICES AVAILABLE",		 0,	10, /* NO_BD_A */
		"SYSTEM NOT IN Systems FILE",	 0,	13, /* BAD_SYS */
		"UNKNOWN dial() FAILURE",	 0,	0
	};

	register struct problem	*p;

	register char		*msg;

#define PREFIX	"Connect problem: "
#define SUFFIX	"This problem has occurred several times.\nPlease check the dialing instructions for this printer.\n"


	for (p = problems; p->dial_error; p++)
		if (p->dial_error == rc)
			break;

	if (!p->retry_max) {
		msg = Malloc(strlen(PREFIX) + strlen(p->reason) + 2);
		sprintf (msg, "%s%s\n", PREFIX, p->reason);
		printer_fault (pps, prs, msg, 0);
		Free (msg);

	} else if (pps->last_dial_rc != rc) {
		pps->nretry = 1;
		pps->last_dial_rc = (short)rc;

	} else if (pps->nretry++ > p->retry_max) {
		pps->nretry = 0;
		pps->last_dial_rc = (short)rc;
		msg = Malloc(
		strlen(PREFIX) + strlen(p->reason) + strlen(SUFFIX) + 1
		);
		sprintf (msg, "%s%s%s\n", PREFIX, p->reason, SUFFIX);
		printer_fault (pps, prs, msg, 0);
		Free (msg);
	}

	if (!(pps->status & PS_FAULTED)) {
		load_str (&pps->dis_reason, p->reason);
		schedule (EV_LATER, WHEN_PRINTER, EV_ENABLE, pps);
	}

	return;
}
