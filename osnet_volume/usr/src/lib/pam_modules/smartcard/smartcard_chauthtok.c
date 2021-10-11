/*
 * Copyright (c) 1992-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)smartcard_chauthtok.c	1.2	99/05/18 SMI"	/* PAM 2.6 */

#include "unix_headers.h"

/*
 * pam_sm_chauthtok():
 *	To change authentication token.
 *
 *	This function handles all requests from the "passwd" command
 *	to change a user's password in all repositories specified
 *	in nsswitch.conf.
 */

int
pam_sm_chauthtok(
	pam_handle_t		*pamh,
	int			flags,
	int			argc,
	const char		**argv)
{
	int i;
	int debug = 0;			/* debug option from pam.conf */
	int authtok_aged = 0;		/* flag to check if password expired */
	unix_authtok_data *status;	/* status in pam handle stating if */
					/* password aged */

	/*
	 * Only check for debug here - parse remaining options
	 * in __update_authtok();
	 */
	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "debug") == 0)
			debug = 1;
	}

	if (flags & PAM_PRELIM_CHECK) {
		/* do not do any prelim check at this time */
		if (debug)
			syslog(LOG_DEBUG,
			"unix pam_sm_chauthtok(): prelim check");
		return (PAM_SUCCESS);
	}

	/* make sure PAM framework is telling us to update passwords */
	if (!(flags & PAM_UPDATE_AUTHTOK)) {
		syslog(LOG_ERR, "unix pam_sm_chauthtok: bad flags: %d", flags);
		return (PAM_SYSTEM_ERR);
	}

	if (flags & PAM_CHANGE_EXPIRED_AUTHTOK) {
		if (pam_get_data(pamh,
				UNIX_AUTHTOK_DATA,
				(const void **)&status) == PAM_SUCCESS) {
			switch (status->age_status) {
			case PAM_NEW_AUTHTOK_REQD:
				if (debug)
				    syslog(LOG_DEBUG,
				    "pam_sm_chauthtok: System password aged");
				authtok_aged = 1;
				break;
			default:
				/* UNIX authtok did not expire */
				if (debug)
				    syslog(LOG_DEBUG,
				    "pam_sm_chauthtok: System password young");
				authtok_aged = 0;
				break;
			}
		}
		if (!authtok_aged)
			return (PAM_IGNORE);
	}

	/*
	 * 	This function calls __update_authtok() to change passwords.
	 *	By passing PAM_REP_DEFAULT, the repository will be determined
	 *	by looking in nsswitch.conf.
	 *
	 *	To obtain the domain name (passed as NULL), __update_authtok()
	 *	will call: nis_local_directory();
	 */
	return (__update_authtok(pamh, flags, PAM_REP_DEFAULT, NULL,
			argc, argv));
}
