/*
*ident	"@(#)td_error.c	1.32	96/05/29 SMI"
*
* Copyright 1993, 1994 by Sun Microsystems, Inc.
*/


/*
*
* Description:
*   Error reporting functions.
*/

#include <thread_db.h>

#ifdef TD_INTERNAL_TESTS
#include "td.h"

/*
* Description:
*   Write to stderr
*
* Input:
*   s1 - string to write
*   s2 - string to write
*
* Output:
*   none
*
* Side effects:
*   Write to stderr
*/
static int
report_err(char *s1, char *s2)
{
	if (td_report_error) {
		if (s2) {
			return (fprintf(stderr,
				"libthread_db: %s %s\n", s1, s2));
		} else {
			return (fprintf(stderr, "libthread_db: %s", s1));
		}
	}
	return (0);
}


/*
* Description:
*   Report errors from debugger services.
*
* Input:
*   error - error number
*   s - string to be added to report.
*
* Output:
*   none
*
* Side effects:
*   none
*/
void
__td_report_db_err(ps_err_e error, char *s)
{

	switch (error) {
	case PS_ERR:
		(void) report_err("Generic imported interface error: ", s);
		break;
	case PS_OK:
		break;
	case PS_BADPID:
		(void) report_err("Bad Process id: ", s);
		break;
	case PS_BADLID:
		(void) report_err("Bad LWP id: ", s);
		break;
	case PS_BADADDR:
		(void) report_err("Bad address on process read/write: ", s);
		break;
	case PS_NOSYM:
		(void) report_err("Symbol not found: ", s);
		break;
	case PS_NOFREGS:
		(void) report_err("No FP registers: ", s);
		break;
	default:
		(void) report_err("Unknown imported interface error: ", s);
	}

}


/*
* Description:
*   Report errors from process object services.
*
* Input:
*   error - error number
*   s - string to be added to report.
*
* Output:
*   none
*
* Side effects:
*   none
*/
void
__td_report_po_err(td_err_e error, char *s)
{

	switch (error) {
		case TD_ERR:
		(void) report_err("Generic process object error: ", s);
		break;
	case TD_OK:
		(void) report_err(": ", s);
		break;
	case TD_NOTHR:
		(void) report_err("Unknown thread object: ", s);
		break;
	case TD_NOSV:
		(void) report_err("Unknown sync. object: ", s);
		break;
	case TD_NOLWP:
		(void) report_err("Invalid LWP id: ", s);
		break;
	case TD_BADPH:
		(void) report_err("Invalid process object: ", s);
		break;
	case TD_BADTH:
		(void) report_err("Invalid thread object: ", s);
		break;
	case TD_BADSH:
		(void) report_err("Invalid sync. object: ", s);
		break;
	case TD_BADKEY:
		(void) report_err("Invalid TSD key: ", s);
		break;
	case TD_NOMSG:
		(void) report_err("No event message: ", s);
		break;
	case TD_NOFPREGS:
		(void) report_err("FP register set not available: ", s);
		break;
	case TD_NOLIBTHREAD:
		(void) report_err("Application does not use libthread: ", s);
		break;
	case TD_NOEVENT:
		(void) report_err("Event not supported: ", s);
		break;
	case TD_NOCAPAB:
		(void) report_err("Capability not implemented: ", s);
		break;
	case TD_DBERR:
		(void) report_err("Imported interface error: ", s);
		break;
	case TD_NOAPLIC:
		(void) report_err("Operation not applicable: ", s);
		break;
	default:
		(void) report_err("Unknown process error: ", s);
	}

}

/*
* Description:
*   Report errors from thread object services.
*
* Input: 	error - error number
*   	s - string to be added to report.
*
* Output: 	none
*
* Side effects:
*   none
*/
void
__td_report_to_err(td_err_e error, char *s)
{

	switch (error) {
	case TD_ERR:
		(void) report_err("Generic thread object error: ", s);
		break;
	case TD_OK:
		(void) report_err(": ", s);
		break;
	default:
		(void) report_err("Unknown thread error: ", s);
	}

}


/*
* Description:
*   Report errors from synch. object services.
*
* Input:
*   error - error number
*   s - string to be added to report.
*
* Output:
*   none
*
* Side effects:
*   none
*/
void
__td_report_so_err(td_err_e error, char *s)
{

	switch (error) {
		case TD_ERR:
		(void) report_err("Generic sync. object error: ", s);
		break;
	case TD_OK:
		(void) report_err(": ", s);
		break;
	default:
		(void) report_err("Unknown thread error: ", s);
	}

}

#endif	/* TD_INTERNAL_TESTS */
