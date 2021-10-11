/*
 * Copyright (c) 1992-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)unix_set_authtokattr.c	1.7	99/07/07 SMI"

#include	"unix_headers.h"

/*
 * __set_authtoken_attr():
 *	To set authentication token attribute values.
 *
 * 	This function calls ck_perm() to check the caller's
 *	permission.  If the check succeeds, It will
 *	call update_authentok_file() and passes attributes/value
 *	pairs pointed by "sa_setattr" to set the authentication
 *	token attribute values of the user specified by the
 *	authentication handle "pamh".
 */

int
__set_authtoken_attr(
	pam_handle_t	*pamh,
	const char	**sa_setattr,
	int		repository,
	const char	*domain,
	int		argc,
	const char	**argv)
{
	register int	i;
	int		retcode;
	char 		*usrname;
	char 		*prognamep;
	struct passwd	*pwd = NULL;
	struct spwd	*shpwd = NULL;
	int		privileged = 0;
	int		debug = 0;
	int		nowarn = 0;
	void		*passwd_res;

	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "debug") == 0)
			debug = 1;
		else if (strcmp(argv[i], "nowarn") == 0)
			nowarn = 1;
		else
			syslog(LOG_ERR, "illegal UNIX module option %s",
				argv[i]);
	}


	if ((retcode = pam_get_item(pamh, PAM_SERVICE, (void **)&prognamep))
						!= PAM_SUCCESS ||
	    (retcode = pam_get_item(pamh, PAM_USER, (void **)&usrname))
						!= PAM_SUCCESS)
		return (retcode);

	if (debug)
		syslog(LOG_DEBUG,
			"__set_authtoken_attr(): repository %x, usrname %s",
			repository, usrname);

	retcode = ck_perm(pamh, repository,
			(char *)domain, &pwd, &shpwd, &privileged,
			(void **)&passwd_res, getuid(), debug, nowarn);
	if (retcode != 0) {
		return (retcode);
	}


	/*
	 * XXX: why do this???
	 * ignore all the signals
	 */
	for (i = 1; i < NSIG; i++)
		(void) sigset(i, SIG_IGN);

	/* update authentication token file */
	/* make sure the user exists before we update the repository */
#ifdef PAM_LDAP
	if (IS_LDAP(repository) && (pwd != NULL)) {
		retcode = update_authtok_ldap(pamh, "attr",
					(char **)sa_setattr, NULL, NULL, pwd,
					privileged, debug, nowarn);
		free_passwd_structs(pwd, shpwd);
		return (retcode);
	} else
#endif
#ifdef PAM_NIS
	if (IS_NIS(repository) && (pwd != NULL)) {
		retcode = update_authtok_nis(pamh, "attr",
		    (char **)sa_setattr, NULL, NULL, pwd,
		    privileged, nowarn);
		free_passwd_structs(pwd, shpwd);
		return (retcode);
	} else
#endif
#ifdef PAM_NISPLUS
	if (IS_NISPLUS(repository) && (pwd != NULL)) {
		/* nis+ needs clear versions of old and new passwds */
		retcode = update_authtok_nisplus(pamh,
		    (char *)domain, "attr", (char **)sa_setattr,
		    NULL, NULL, NULL, IS_OPWCMD(repository) ? 1 : 0, pwd,
		    privileged, (nis_result *)passwd_res, NULL,
		    debug, nowarn, TRUE /* not used */);

		free_passwd_structs(pwd, shpwd);
		return (retcode);
	} else
#endif
	if (IS_FILES(repository) && (pwd != NULL)) {
		retcode = update_authtok_file(pamh, "attr",
			(char **)sa_setattr, pwd,
			privileged, nowarn);
		free_passwd_structs(pwd, shpwd);
		return (retcode);
	}

	return (PAM_SUCCESS);
}
