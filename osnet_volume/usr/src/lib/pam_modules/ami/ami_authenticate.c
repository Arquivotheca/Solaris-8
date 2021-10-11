/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)ami_authenticate.c	1.1 99/07/11 SMI"
 */

#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include "ami_auth_headers.h"

static int attempt_authentication(pam_handle_t *pamh, char *user,
    char *password, int flags, int debug);

int
pam_sm_authenticate(
	pam_handle_t            *pamh,
	int                     flags,
	int                     argc,
	const char              **argv)
{
	int	debug = 0;
	int	try_first_pass = 0;
	int	use_first_pass = 0;
	int     err = PAM_SUCCESS;
	char    *service = NULL;
	char	*user = NULL;
	char	*password = NULL;
	int     i;

	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "debug") == 0)
			debug = 1;
		else if (strcmp(argv[i], "try_first_pass") == 0)
			try_first_pass = 1;
		else if (strcmp(argv[i], "use_first_pass") == 0)
			use_first_pass = 1;
		else if (strcmp(argv[i], "nowarn") == 0)
			flags = flags | PAM_SILENT;
		else {
			/* Ignore unrecognized arguments */
			syslog(LOG_ERR, "illegal option %s", argv[i]);
		}
	}

	if ((err = pam_get_item(pamh, PAM_SERVICE, (void **)&service))
			!= PAM_SUCCESS ||
	    (err = pam_get_user(pamh, &user, NULL)) != PAM_SUCCESS)
		return (err);

	if (debug)
		syslog(LOG_DEBUG,
			"AMI pam_sm_authenticate(%s %s), flags = %x ",
			service, (user)?user:"no-user", flags);

	if (!user)
		return (PAM_USER_UNKNOWN);

	if ((err = __pam_get_authtok(pamh, PAM_HANDLE, PAM_AUTHTOK,
		PASSWORD_LEN, NULL, &password, NULL)) != PAM_SUCCESS)
		goto out;

	if (try_first_pass) {

		/*
		 * Try to login using the password from the first
		 * scheme, e.g. UNIX password. If anything goes wrong,
		 * then simply prompt users for password.
		 */
		err = attempt_authentication(pamh, user, password,
				flags, debug);

		if (err == PAM_SUCCESS)
			goto post_prompt;
		else
			goto prompt;

	} else if (use_first_pass) {

		/*
		 * Try to login using the password from the first
		 * scheme, e.g. UNIX password. If anything goes wrong,
		 * quit, and return the error.
		 */
		err = attempt_authentication(pamh, user, password,
				flags, debug);

		if (err == PAM_SUCCESS)
			goto post_prompt;
		else
			goto out;
	}

prompt:
	/*
	 * Get the password from the user
	 */

	if (password) {
		memset(password, 0, strlen(password));
		free(password);
		password = NULL;
	}
	if ((err = __pam_get_authtok(pamh, PAM_PROMPT,
		PAM_AUTHTOK, PASSWORD_LEN,
		PAM_MSG(pamh, 10, "AMI Password: "),
		&password, NULL)) != PAM_SUCCESS) {
		goto out;
	}
		
	err = attempt_authentication(pamh, user, password,
		flags, debug);

	if (err != PAM_SUCCESS)
		goto out;

post_prompt:
	/*
	 * Shall we register the keypkg here or in the pam_sm_setcred()?
	 */
out:
	if (password) {
		memset(password, 0, strlen(password));
		free(password);
		password = NULL;
	}

	return (err);
}

static int
attempt_authentication(pam_handle_t *pamh, char *user, char *password,
	int flags, int debug)
{
	struct          passwd pwd; /* password structure */
	char            pwd_buf[1024];
	int		retcode = 0;

	if (password == NULL) {
		if (debug)
			syslog(LOG_DEBUG,
			"ami_auth: NULL passwd in attempt_authenticate()");
		return (PAM_AUTH_ERR);
	}

	/* pwd.pw_uid will be passed to ami_attempt_authentication */
	if (getpwnam_r(user, &pwd, pwd_buf, sizeof (pwd_buf)) == NULL)
		return(PAM_USER_UNKNOWN);

	if (debug)
		syslog(LOG_DEBUG,
			"Ready to call ami_attemp_authentication");

	retcode = ami_attempt_authentication(pwd.pw_uid,
					     pwd.pw_name,
					     password,
				 	     flags,
					     debug);

	return (retcode);
}	
