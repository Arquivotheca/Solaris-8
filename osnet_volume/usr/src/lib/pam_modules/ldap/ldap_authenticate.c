/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)ldap_authenticate.c	1.2	99/07/14 SMI"

#include "ldap_headers.h"

/*
 *
 * LDAP module for pam_sm_authenticate.
 *
 * options -
 *
 *	debug
 *	use_first_pass
 *	try_first_pass
 */

/*
 * pam_sm_authenticate():
 * 	Authenticate user. If try_first_pass or use_first_pass is set in
 *	the /etc/pam.conf file, then the first password from the stack
 *	is used in attempting authentication. Otherwise, the user is
 *	prompted for a new password.
 *	This function calls authenticate.
 */
/*ARGSUSED*/
int
pam_sm_authenticate(
	pam_handle_t		*pamh,
	int 			flags,
	int			argc,
	const char		**argv)
{
	char			*service = NULL;
	char			*user = NULL;
	struct pam_conv 	*pam_convp = NULL;
	int			err;
	int			result = PAM_AUTH_ERR;
	struct pam_response 	*ret_resp = NULL;
	char 			messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];
	int			debug = 0;
	int			try_first_pass = 0;
	int			use_first_pass = 0;
	int			i;
	int			num_msg = -1;
	char			*firstpass = NULL;
	char			*password = NULL;
	Auth_t			*authp = NULL;

	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "debug") == 0)
			debug = 1;
		else if (strcmp(argv[i], "try_first_pass") == 0)
			try_first_pass = 1;
		else if (strcmp(argv[i], "use_first_pass") == 0)
			use_first_pass = 1;
		else
			syslog(LOG_DEBUG, "illegal scheme option %s", argv[i]);
	}

	if ((err = pam_get_item(pamh, PAM_SERVICE, (void **)&service))
							!= PAM_SUCCESS ||
	    (err = pam_get_user(pamh, &user, NULL)) != PAM_SUCCESS)
		return (err);

	if (debug)
		syslog(LOG_DEBUG,
			"ldap pam_sm_authenticate(%s %s), flags = %x ",
			service, (user)?user:"no-user", flags);

	if (!user)
		return (PAM_USER_UNKNOWN);


	err = pam_get_item(pamh, PAM_CONV, (void**) &pam_convp);
	if (err != PAM_SUCCESS)
		return (err);

	/* Get the password entered in the first scheme if any */
	(void) pam_get_item(pamh, PAM_AUTHTOK, (void **) &firstpass);

	if (try_first_pass) {
		/*
		 * Try to login using the password from the first
		 * scheme, e.g. UNIX password. If anything goes wrong,
		 * then simply prompt users for the LDAP password.
		 */

		if (firstpass == NULL)
			goto prompt;
		else {
			/* Try to authenticate with first password */
			result = authenticate(&authp, user, firstpass);
			if (result == PAM_SUCCESS)
				goto out;
			else
				goto prompt;

		} /* firspass == NULL */

	} else if (use_first_pass) {
		/*
		 * Try to login using the password from the first
		 * scheme, e.g. UNIX password. If anything goes wrong,
		 * quit, and return the error;
		 */

		if (firstpass == NULL)
			goto out;
		else {
			/* Try to authenticate with first password */
			result = authenticate(&authp, user, firstpass);
			goto out;

		} /* firspass == NULL */

	}

prompt:
	/* Get the password from the user */
	if (try_first_pass) {
		(void) snprintf(messages[0],
				sizeof (messages[0]),
				(const char *) PAM_MSG(pamh, 30,
				"LDAP Password: "));
	} else {
		(void) snprintf(messages[0],
				sizeof (messages[0]),
				(const char *) PAM_MSG(pamh, 31,
				"Password: "));
	}

	num_msg = 1;
	if ((result = __get_authtok(pam_convp->conv, num_msg, messages,
				    NULL, &ret_resp)) != PAM_SUCCESS)
		goto out;

	password = ret_resp->resp;

	if (password == NULL) {
		result = PAM_AUTH_ERR;
		goto out;
	}

	result = authenticate(&authp, user, password);

	if (firstpass == NULL) {
		/* this is the first password, stash it away */
		pam_set_item(pamh, PAM_AUTHTOK, password);
	}

out:
	/* Cleaning up */
	if (num_msg > 0) {
		if (ret_resp != 0) {
			if (ret_resp->resp != 0) {
				/* avoid leaving password cleartext around */
				memset(ret_resp->resp, 0,
					strlen(ret_resp->resp));
			}
			__free_resp(num_msg, ret_resp);
			ret_resp = 0;
		}
	}

	__ns_ldap_freeAuth(&authp);

	return (result);
}
