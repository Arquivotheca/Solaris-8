/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)ldap_chauthtok.c	1.1	99/07/07 SMI"

#include "ldap_headers.h"

/*
 * pam_sm_chauthtok():
 *	To change authentication token.
 *
 *	This function handles all requests from the "passwd" command
 *	to change a user's password in the ldap repository
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

	/*
	 * Only check for debug here - parse remaining options
	 * in __update_passwd();
	 */
	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "debug") == 0) {
			debug = 1;
			break;
		}
	}

	if (flags & PAM_PRELIM_CHECK) {
		/* do not do any prelim check at this time */
		if (debug)
			syslog(LOG_DEBUG,
			"pam_ldap pam_sm_chauthtok(): prelim check");
		return (PAM_SUCCESS);
	}

	/* make sure PAM framework is telling us to update passwords */
	if (!(flags & PAM_UPDATE_AUTHTOK)) {
		syslog(LOG_ERR,
			"pam_ldap pam_sm_chauthtok: bad flags: %d", flags);
		return (PAM_SYSTEM_ERR);
	}

	/*
	 * Later, we may need to insert code here which checks for passwd
	 * aging as in pam_unix. As of now, we rely on the directory
	 * server to enforce password aging.
	 */

	/* change passwords.in the ldap repository if user exists */
	return (__update_passwd(pamh, flags, argc, argv));
}
