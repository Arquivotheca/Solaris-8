/*
 * Copyright (c) 1992-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)rhosts_auth.c	1.5	98/06/14 SMI"	/* PAM 2.6 */

#include <sys/param.h>
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <pwd.h>
#include <shadow.h>
#include <string.h>
#include <rpc/types.h>
#include <rpc/auth.h>
#include <locale.h>
#include <crypt.h>
#include <syslog.h>

extern int ruserok(const char *, int, const char *, const char *);

/*
 * pam_sm_authenticate	- Checks if the user is allowed remote access
 */
/*ARGSUSED*/
int
pam_sm_authenticate(
	pam_handle_t	*pamh,
	int	flags,
	int	argc,
	const char	**argv)
{
	char *host = NULL, *lusername = NULL;
	struct passwd pwd;
	char pwd_buffer[1024];
	int	is_superuser;
	char	*rusername;
	int	i;
	int	debug = 0;

	for (i = 0; i < argc; i++) {
		if (strcasecmp(argv[i], "debug") == 0)
			debug = 1;
		else
			syslog(LOG_DEBUG, "illegal option %s", argv[i]);
	}

	if (pam_get_item(pamh, PAM_USER, (void **) &lusername) != PAM_SUCCESS)
		return (PAM_SERVICE_ERR);
	if (pam_get_item(pamh, PAM_RHOST, (void **) &host) != PAM_SUCCESS)
		return (PAM_SERVICE_ERR);
	if (pam_get_item(pamh, PAM_RUSER, (void **)&rusername) != PAM_SUCCESS)
		return (PAM_SERVICE_ERR);

	if (debug) {
		syslog(LOG_DEBUG,
			"rhosts authenticate: user = %s, host = %s",
			lusername, host);
	}

	if (getpwnam_r(lusername, &pwd, pwd_buffer, sizeof (pwd_buffer))
								== NULL)
		return (PAM_USER_UNKNOWN);

	/*
	 * RHOST may not be set due to unknown USER or reset by previous
	 * authentication failure.
	 */
	if ((rusername == NULL) || (rusername[0] == '\0'))
		return (PAM_AUTH_ERR);

	if (pwd.pw_uid == 0)
		is_superuser = 1;
	else
		is_superuser = 0;

	return (ruserok(host, is_superuser, rusername, lusername)
		== -1 ? PAM_AUTH_ERR : PAM_SUCCESS);

}

/*
 * dummy pam_sm_setcred - does nothing
 */
/*ARGSUSED*/
pam_sm_setcred(
	pam_handle_t	*pamh,
	int	flags,
	int	argc,
	const char	**argv)
{
	return (PAM_SUCCESS);
}
