
/*
 * Copyright (c) 1992-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)dial_auth.c	1.6	98/06/14 SMI"

#include <sys/param.h>
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include "../../libpam/pam_impl.h"
#include <pwd.h>
#include <shadow.h>
#include <string.h>
#include <rpc/types.h>
#include <rpc/auth.h>
#include <locale.h>
#include <crypt.h>
#include <syslog.h>

/*
 * Various useful files and string constants
 */
#define	DIAL_FILE	"/etc/dialups"
#define	DPASS_FILE	"/etc/d_passwd"
#define	SHELL		"/usr/bin/sh"
#define	SCPYN(a, b)	(void) strncpy(a, b, sizeof (a))
#define	PASSWORD_LEN	8

/*
 * PAM_MSG macro for return of internationalized text
 */

#define	PAM_MSG(pamh, number, string)\
	(char *) __pam_get_i18n_msg(pamh, "pam_unix", 2, number, string)

/*
 * pam_sm_authenticate	- This is the top level function in the
 *			module called by pam_auth_port in the framework
 *			Returns: PAM_AUTH_ERR on failure, 0 on success
 */
/*ARGSUSED*/
int
pam_sm_authenticate(
	pam_handle_t	*pamh,
	int	flags,
	int	argc,
	const char	**argv)
{
	char	*ttyn, *user;
	FILE 	*fp;
	char 	defpass[30];
	char	line[80];
	char 	*p1 = 0, *p2 = 0;
	struct passwd 	pwd;
	char	pwd_buffer[1024];
	char	*password = 0;
	int	retcode;
	int	i;
	int	debug = 0;

	for (i = 0; i < argc; i++) {
		if (strcasecmp(argv[i], "debug") == 0)
			debug = 1;
		else
			syslog(LOG_DEBUG, "illegal option %s", argv[i]);
	}

	if ((retcode = pam_get_user(pamh, &user, NULL))
					!= PAM_SUCCESS ||
	    (retcode = pam_get_item(pamh, PAM_TTY, (void **)&ttyn))
					!= PAM_SUCCESS)
		return (retcode);

	if (debug) {
		syslog(LOG_DEBUG,
			"Dialpass authenticate user = %s, ttyn = %s",
			user, ttyn);
	}

	if (getpwnam_r(user, &pwd, pwd_buffer, sizeof (pwd_buffer)) == NULL)
		return (PAM_USER_UNKNOWN);

	if ((fp = fopen(DIAL_FILE, "r")) == NULL) {
		return (PAM_SUCCESS);
	}

	while ((p1 = fgets(line, sizeof (line), fp)) != NULL) {
		while (*p1 != '\n' && *p1 != ' ' && *p1 != '\t')
			p1++;
		*p1 = '\0';
		if (strcmp(line, ttyn) == 0)
			break;
	}

	(void) fclose(fp);

	if (p1 == NULL || (fp = fopen(DPASS_FILE, "r")) == NULL)
		return (PAM_SUCCESS);

	defpass[0] = '\0';
	p2 = 0;

	while ((p1 = fgets(line, sizeof (line)-1, fp)) != NULL) {
		while (*p1 && *p1 != ':')
			p1++;
		*p1++ = '\0';
		p2 = p1;
		while (*p1 && *p1 != ':')
			p1++;
		*p1 = '\0';
		if (pwd.pw_shell != NULL && strcmp(pwd.pw_shell, line) == 0)
			break;

		if (strcmp(SHELL, line) == 0)
			SCPYN(defpass, p2);
		p2 = 0;
	}

	(void) fclose(fp);

	if (!p2)
		p2 = defpass;

	if (*p2 != '\0') {
		if ((retcode = __pam_get_authtok(pamh, PAM_PROMPT,
			PAM_AUTHTOK, PASSWORD_LEN,
			PAM_MSG(pamh, 1, "Dialup Password: "),
			&password, NULL)) != PAM_SUCCESS) {
			/*EMPTY*/
		}

		if (strcmp(crypt(password, p2), p2)) {
			return (PAM_AUTH_ERR);
		}
	}

	return (PAM_SUCCESS);

}

/*
 * dummy pam_sm_setcred - does nothing
 */
/*ARGSUSED*/
pam_sm_setcred(
	pam_handle_t    *pamh,
	int	flags,
	int	argc,
	const char    **argv)
{
	return (PAM_SUCCESS);
}
