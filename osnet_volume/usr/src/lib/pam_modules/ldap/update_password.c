
/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)update_password.c	1.2	99/07/14 SMI"

#include <deflt.h>	/* for defopen, defread */
#include "ldap_headers.h"

static int 	get_ns(pam_handle_t *, int, int);
static void	pr_config(pam_handle_t *);
static int	verify_old_passwd(pam_handle_t *, struct spwd *,
				char *, uid_t, int *, int *, int, int);
static int	get_newpasswd(pam_handle_t *, char *, char *,
				uid_t, int, int, int, int);
static int	circ(char *, char *);
static int	triviality_checks(pam_handle_t *, uid_t,
					char *, char *, int);
static int	change_password(pam_handle_t *, char *, char *,
				struct passwd *, int, int);

/*
 * __update_passwd():
 *	To change authentication token.
 *
 * 	This function calls ck_perm() to check the caller's
 *	permission.  If the check succeeds, it will then call
 *	verify_old_passwd() to validate the old password.
 *	If verify_old_passwd() succeeds, get_newpasswd()
 *	will then be called to get and check the user's new passwd.
 *	Last, change_password() will be called to change the user's
 *	password to the new password.
 *
 *	All temporary password buffers allocate PAM_MAX_RESP_SIZE bytes.
 *	This is because the longest value (and hence password) that can be
 *	returned by a PAM conv function is PAM_MAX_RESP_SIZE bytes.
 *
 *	This function is called by pam_sm_chauthtok to change the password
 *	in the ldap repository.
 *
 */
/*ARGSUSED*/
int
__update_passwd(
	pam_handle_t	*pamh,
	int		flags,
	int		argc,
	const char	**argv)
{
	int		retcode;
	int		i;
	struct passwd	*pwd = NULL;
	struct spwd	*shpwd = NULL;
	char 		*usrname = NULL;
	char 		*prognamep = NULL;
	uid_t		uid;		/* real uid of calling process */
	char	opwbuf[PAM_MAX_RESP_SIZE] = {'\0'};	/* old passwd */
	char	pwbuf[PAM_MAX_RESP_SIZE] = {'\0'};	/* new passwd */
	char		messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];
	int		debug = 0;
	int		nowarn = 0;
	int		try_first_pass = 0;
	int		use_first_pass = 0;
	int		repository = PAM_REP_DEFAULT;

#ifdef DEBUG
	fprintf(stderr, "\n[update_password.c]\n");
	fprintf(stderr, "\t__update_passwd()\n");
#endif

	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "debug") == 0)
			debug = 1;
		else if (strcmp(argv[i], "nowarn") == 0)
			nowarn = 1;
		else if (strcmp(argv[i], "try_first_pass") == 0)
			try_first_pass = 1;
		else if (strcmp(argv[i], "use_first_pass") == 0)
			use_first_pass = 1;
		else
			syslog(LOG_ERR,
				"LDAP chauthtok(): illegal module option %s",
				argv[i]);
	}

	if (debug) {
		syslog(LOG_DEBUG, "LDAP: __update_passwd(): update passwords");
		syslog(LOG_DEBUG, "LDAP: __update_passwd():"
			"uid = %d, euid = %d", getuid(), geteuid());
	}

	if ((retcode = pam_get_item(pamh, PAM_SERVICE, (void **)&prognamep))
						!= PAM_SUCCESS ||
	    (retcode = pam_get_item(pamh, PAM_USER, (void **)&usrname))
						!= PAM_SUCCESS) {
		goto out;
	}

	/*
	 * get_ns() will consult nsswitch file and set the repository
	 * accordingly.
	 */
	repository = get_ns(pamh, debug, nowarn);
	if (repository == -1) {
		retcode = PAM_AUTHTOK_ERR;
		goto out;
	}
	if (debug)
		syslog(LOG_DEBUG,
		"LDAP: __update_passwd(): repository: %s after get_ns()",
			repository_to_string(repository));

	/* if we still get PAM_REP_DEFAULT after calling get_ns(), error! */
	if (repository == PAM_REP_DEFAULT) {
		retcode = PAM_AUTHTOK_ERR;
		goto out;
	}

	/*
	 * If repository is not ldap return as we do not try
	 * to update auth token for other repositories in this module.
	 */
	if (repository != PAM_REP_LDAP) {
		retcode = PAM_AUTHTOK_ERR;
		goto out;

	}


	retcode = ck_perm(pamh, repository, &pwd, &shpwd,
				uid, debug, nowarn);

	if (retcode != PAM_SUCCESS) {
		if (retcode == PAM_USER_UNKNOWN) {
			if (!nowarn) {
				snprintf(messages[0],
					sizeof (messages[0]),
					PAM_MSG(pamh, 11,
				"%s%s: %s does not exist"),
					prognamep,
					LDAP_MSG,
					usrname);
				(void) __pam_display_msg(
					pamh,
					PAM_ERROR_MSG, 1,
					messages, NULL);
			}
		}
		goto out;
	}

	/* verify the old password */
	retcode = verify_old_passwd(pamh, shpwd, opwbuf, uid, &try_first_pass,
				    &use_first_pass, debug, nowarn);
	if (retcode != PAM_SUCCESS)
		goto out;

	retcode = get_newpasswd(pamh, pwbuf, opwbuf, uid, try_first_pass,
				use_first_pass, debug, nowarn);
	if (retcode != PAM_SUCCESS)
		goto out;

	if (pwd != NULL)
		retcode = change_password(pamh, opwbuf, pwbuf,
						pwd, debug, nowarn);

out:
	memset(pwbuf, 0, sizeof (pwbuf));
	memset(opwbuf, 0, sizeof (opwbuf));

	/*
	 * This function will free the pwd & shpwd structures after checking
	 * if they are valid or not. So, a check for NULL ptr here is not
	 * necessary.
	 */
	free_passwd_structs(pwd, shpwd);

	return (retcode);
}

/*
 * get_ns():
 * 	get name services (or repositories) of passwd.
 * 	return value: new repositories from nsswitch as long as it represents
 *	 a valid and supported configuration. If we cannot find
 *	 a name service switch entry PAM_REP_DEFAULT as default is returned.
 */
static int
get_ns(
	pam_handle_t 	*pamh,
	int 		debug,
	int 		nowarn)
{
	struct __nsw_switchconfig *conf = NULL;
	struct __nsw_switchconfig *confcomp = NULL;
	enum __nsw_parse_err pserr;
	struct __nsw_lookup *lkp;
	struct __nsw_lookup *lkp2;
	int	rep = PAM_REP_DEFAULT;
	char messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];

#ifdef DEBUG
	fprintf(stderr, "\n[update_password.c]\n");
	fprintf(stderr, "\tget_ns()\n");
#endif

	conf = __nsw_getconfig("passwd", &pserr);
	if (conf == NULL) {
		if (!nowarn) {
			snprintf(messages[0],
				sizeof (messages[0]),
				PAM_MSG(pamh, 12,
					"Can't find name service for passwd"));
			__pam_display_msg(pamh, PAM_ERROR_MSG,
				1, messages, NULL);
		}

		return (rep);	/* default */
	}

	if (debug) {

		syslog(LOG_DEBUG, "number of services is %d",
			conf->num_lookups);
	}

	lkp = conf->lookups;

	/* Currently we do not support more than 2 services */
	if (conf->num_lookups > 2) {
		pr_config(pamh);
		return (-1);
	} else if (conf->num_lookups == 1) {
		/* files or compat */
		if (strcmp(lkp->service_name, "files") == 0) {
			rep |= PAM_REP_FILES;
			return (rep);
		} else if (strcmp(lkp->service_name, "compat") == 0) {

			/* get passwd_compat */
			confcomp = __nsw_getconfig("passwd_compat", &pserr);
			if (confcomp == NULL) {
				rep = PAM_REP_DEFAULT;
				return (rep);
			} else {
				/* check the service: nisplus? */
				if (strcmp(confcomp->lookups->service_name,
				    "nisplus") == 0) {
					rep = PAM_REP_FILES | PAM_REP_NISPLUS;
					return (rep);
				} else if (strcmp(
					confcomp->lookups->service_name,
					"ldap") == 0) {
					/*
					 * Service is ldap. Even though
					 * files also should be in the rep.
					 * it is not returned as in pam_ldap
					 * we do not sync with files as in
					 * pam_unix.
					 */
					rep = PAM_REP_LDAP;
					return (rep);
				} else {
				/* passwd_compat must be nisplus or ldap */
					return (-1);
				}
			}
		} else {
			pr_config(pamh);
			return (-1);
		}
	} else  { /* two services */
		lkp = conf->lookups;
		lkp2 = lkp->next;
		if (strcmp(lkp->service_name, "files") == 0) {
			/* files ldap, files nis, or files nisplus */
			rep |= PAM_REP_FILES;
			/* continue */
		} else {
			pr_config(pamh);
			return (-1);
		}
		/*
		 * Even though the entry is files, ldap, we only return
		 * LDAP as in pam_ldap, we do not sync with files
		 */
		if (strcmp(lkp2->service_name, "ldap") == 0) {
			rep = PAM_REP_LDAP;
			return (rep);
		} else if (strcmp(lkp2->service_name, "nis") == 0) {
			rep |= PAM_REP_NIS;
			return (rep);
		} else if (strcmp(lkp2->service_name, "nisplus") == 0) {
			rep |= PAM_REP_NISPLUS;
			return (rep);
		} else {
			pr_config(pamh);
			return (-1);
		}
	}
}

/*
 * pr_config():
 *	Prints out error message for the supported configurations in the
 *	/etc/nsswitch.conf file.
 */
static void
pr_config(pam_handle_t *pamh)
{
	char 		messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];

	snprintf(messages[0], sizeof (messages[0]), PAM_MSG(pamh, 1,
	"Supported configurations for passwd management are as follows:"));
	snprintf(messages[1], sizeof (messages[1]), PAM_MSG(pamh, 2,
	"    passwd: files"));
	snprintf(messages[2], sizeof (messages[2]), PAM_MSG(pamh, 3,
	"    passwd: files ldap"));
	snprintf(messages[3], sizeof (messages[3]), PAM_MSG(pamh, 4,
	"    passwd: files nis"));
	snprintf(messages[4], sizeof (messages[4]), PAM_MSG(pamh, 5,
	"    passwd: files nisplus"));
	snprintf(messages[5], sizeof (messages[5]), PAM_MSG(pamh, 6,
	"    passwd: compat"));
	snprintf(messages[6], sizeof (messages[6]), PAM_MSG(pamh, 7,
	"    passwd: compat AND"));
	snprintf(messages[7], sizeof (messages[7]), PAM_MSG(pamh, 8,
	"    passwd_compat: ldap OR"));
	snprintf(messages[8], sizeof (messages[8]), PAM_MSG(pamh, 9,
	"    passwd_compat: nisplus"));
	snprintf(messages[9], sizeof (messages[9]), PAM_MSG(pamh, 10,
	"Please check your /etc/nsswitch.conf file"));

	/* display the above 10 messages */
	(void) __pam_display_msg(pamh, PAM_ERROR_MSG, 10, messages, NULL);
}

/*
 * verify_old_passwd():
 * 	To verify user old password. The manner in which the old password
 *	is verified in LDAP is by using it to authenticate to the LDAP
 *	server. If this succeeds, then the password is correct.
 */

static int
verify_old_passwd(
	pam_handle_t *pamh,
	struct spwd *shpwd,
	char *opwbuf,		/* old password */
	uid_t uid,
	int *try_first_pass,
	int *use_first_pass,
	int debug,
	int nowarn)
{
	int	done = 0;
	int	retcode;
	char	*prognamep;
	char 	messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];
	char	*pswd = NULL;	/* user input clear passwd */
	char	prompt[PAM_MAX_MSG_SIZE];
	/* Set if password already exists as pam handle item */
	int	passwd_already_set = 0;
	char	*saved_passwd = NULL;
	Auth_t  *authp = NULL;

#ifdef DEBUG
	fprintf(stderr, "\n[update_password.c]\n");
	fprintf(stderr, "\tverify_old_password()\n");
#endif

	if (debug)
		syslog(LOG_DEBUG,
		"pam_ldap: verify_old_passwd: start: uid = %d", uid);

	if (debug) {
		syslog(LOG_DEBUG,
			"pam_ldap: verify_old_passwd()");
		syslog(LOG_DEBUG,
			"    try_first_pass = %d, use_first_pass = %d",
			*try_first_pass, *use_first_pass);
	}

	if ((retcode = pam_get_item(pamh, PAM_SERVICE, (void **)&prognamep))
							!= PAM_SUCCESS)
		return (retcode);

	/*
	 * Make sure there is a shadow passwd. This may not have a real
	 * password. It might just be the string *NP* but anyway this
	 * check proves that the user has a password
	 *
	 * This check may not be necessary
	 */
	if (shpwd->sp_pwdp[0]) {

		/*
		 * See if there's a password already saved in the
		 * pam handle.  If not, then if we prompt for a password,
		 * it will be saved in the handle by __pam_get_authtok().
		 * We will want to clear this password from the handle
		 * if it is incorrect before we return.
		 */
		pam_get_item(pamh, PAM_OLDAUTHTOK, (void **)&saved_passwd);
		if (saved_passwd != NULL)
			passwd_already_set = 1;

		/*
		 * The only time this loop will iterate more than once is
		 * if try_first_pass is set and the first password is not
		 * correct so we must prompt the user for a new one.
		 */
		while (!done) {
			if (*try_first_pass || *use_first_pass) {
				__pam_get_authtok(pamh, PAM_HANDLE,
					PAM_OLDAUTHTOK, PASSWORD_LEN,
					0, &pswd, NULL);
			} else {
				memset(prompt, 0, PAM_MAX_MSG_SIZE);
				strcpy(prompt, PAM_MSG(pamh, 13,
					"Enter login(LDAP) password: "));

				retcode = __pam_get_authtok(pamh, PAM_PROMPT,
					PAM_OLDAUTHTOK, PASSWORD_LEN,
					prompt, &pswd, NULL);
				if (retcode != PAM_SUCCESS)
					goto out;
			}

			if (pswd == NULL || pswd[0] == '\0') {
				if (*try_first_pass) {
					/*
					 * This means that the module has
					 * try_first_pass set, but there was no
					 * previous module that ever prompted
					 * for a password.  Go back and prompt
					 * for a password.
					 */
					*try_first_pass = 0;
					continue;
				}

				(void) snprintf(messages[0],
					sizeof (messages[0]),
					PAM_MSG(pamh, 14, "%s%s: Sorry."),
					prognamep, LDAP_MSG);
				(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
					1, messages, NULL);

				if (pswd)
					free(pswd);

				*use_first_pass = 0;
				retcode = PAM_AUTHTOK_ERR;
				goto out;
			} else {
				(void) strncpy(opwbuf, pswd, PAM_MAX_RESP_SIZE);

				memset(pswd, 0, strlen(pswd));
				free(pswd);
			}

			/* Use __ns_ldap_auth to verify password */
			if (authenticate(&authp, shpwd->sp_namp,
					opwbuf) != PAM_SUCCESS) {
				if (*try_first_pass) {
					/*
					 * We just attempted the first
					 * password and it failed.  Go
					 * back and prompt for the old
					 * password.
					 */
					*try_first_pass = 0;
					continue;
				} else {
					(void) snprintf(messages[0],
					sizeof (messages[0]),
					PAM_MSG(pamh, 15,
					"%s%s: Sorry, wrong passwd"),
					prognamep, LDAP_MSG);
					(void) __pam_display_msg(pamh,
					PAM_ERROR_MSG, 1, messages,
					NULL);
					*use_first_pass = 0;

					/* clear bad passwd */
					if (passwd_already_set == 0)
						pam_set_item(pamh,
							PAM_OLDAUTHTOK,
							NULL);

					retcode = PAM_PERM_DENIED;
					goto out;
				}
			}

			/* At this point the old password has been verified. */
			done = 1;
		} /* while */

	} else {
		opwbuf[0] = '\0';
	}

	retcode = PAM_SUCCESS;


out:
	__ns_ldap_freeAuth(&authp);

	/* Add code for aging later */
	return (retcode);
}

/*
 * get_newpasswd():
 * 	Get user's new password. It also does the syntax check for
 * 	the new password.
 */

static int
get_newpasswd(
	pam_handle_t *pamh,
	char pwbuf[PAM_MAX_RESP_SIZE],	/* new password */
	char *opwbuf,		/* old password */
	uid_t uid,   		/* UID of user that invoked passwd cmd */
	int try_first_pass,
	int use_first_pass,
	int debug,
	int nowarn)
{

	int		insist = 0; /* # times new passwd fails checks */
	int		count = 0;  /* # times old/new passwds do not match */
	int		done = 0;	/* continue to prompt until done */
	char		*usrname;
	char		*prognamep;
	int		retcode;
	char 		messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];
	char		*pswd = NULL;

	/* Set if password already exists as pam handle item */
	int		passwd_already_set = 0;
	char		*saved_passwd = NULL;

#ifdef DEBUG
	fprintf(stderr, "\n[update_password.c]\n");
	fprintf(stderr, "\tget_newpassword()\n");
#endif

	if (debug)
		syslog(LOG_DEBUG,
		"get_newpasswd(), try_first_pass = %d, use_first_pass = %d",
		try_first_pass, use_first_pass);

	if ((retcode = pam_get_item(pamh, PAM_SERVICE, (void **)&prognamep))
						!= PAM_SUCCESS ||
	    (retcode = pam_get_item(pamh, PAM_USER, (void **)&usrname))
						!= PAM_SUCCESS)
		return (retcode);

	pam_get_item(pamh, PAM_AUTHTOK, (void **)&saved_passwd);
	if (saved_passwd != NULL)
		passwd_already_set = 1;

	while (insist < MAX_CHANCES && count < MAX_CHANCES && !done) {

		/* clear a bad "new password" if it was saved */
		if (passwd_already_set == 0)
			pam_set_item(pamh, PAM_AUTHTOK, NULL);

		/*
		 * If use_first_pass failed, then return.
		 * If try_first_pass failed, note it so that we
		 * do not try it again (when retyping in the new
		 * password).
		 */
		if (use_first_pass && (insist != 0 || count != 0))
			return (PAM_AUTHTOK_ERR);
		if (try_first_pass && (insist != 0 || count != 0))
			try_first_pass = 0;

		if ((try_first_pass || use_first_pass) &&
			insist == 0 && count == 0) {
			/*
			 * Try the new password entered to the first password
			 * module in the stack (try/use_first_pass option).
			 * If it already failed (insist > 0 || count > 0) then
			 * prompt if option is try_first_pass.
			 */
			if ((retcode = __pam_get_authtok(pamh, PAM_HANDLE,
				PAM_AUTHTOK, PASSWORD_LEN, 0,
				&pswd, NULL)) != PAM_SUCCESS) {
				return (retcode);
			}
		} else {
			if ((retcode = __pam_get_authtok(pamh, PAM_PROMPT,
				PAM_AUTHTOK, PASSWORD_LEN,
				PAM_MSG(pamh, 16, "New password: "),
				&pswd, NULL)) != PAM_SUCCESS) {
					return (retcode);
			}
		}

		if (pswd == NULL) {
			if (try_first_pass) {
				/*
				 * This means that the module has
				 * try_first_pass set, but there was no
				 * previous module that ever prompted for
				 * a password.  Go back and prompt for
				 * a password.
				 */
				count++;
				continue;
			}

			(void) snprintf(messages[0],
				sizeof (messages[0]),
				PAM_MSG(pamh, 14, "%s%s: Sorry."),
				prognamep, LDAP_MSG);
			(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
				1, messages, NULL);

			/* clear a bad "new password" if it was saved */
			if (passwd_already_set == 0)
				pam_set_item(pamh, PAM_AUTHTOK, NULL);
			return (PAM_AUTHTOK_ERR);
		} else {
			memset(pwbuf, 0, PAM_MAX_RESP_SIZE);
			(void) strncpy(pwbuf, pswd, PAM_MAX_RESP_SIZE);

			memset(pswd, 0, PASSWORD_LEN);
			free(pswd);
		}

		if ((retcode = triviality_checks(pamh, uid, opwbuf,
					pwbuf, nowarn)) != PAM_SUCCESS) {
			if (retcode == PAM_AUTHTOK_ERR) {
				insist++;
				continue;
			} else {
				/* clear bad "new password" if saved */
				if (passwd_already_set == 0)
					pam_set_item(pamh, PAM_AUTHTOK, NULL);
				return (retcode);
			}
		}

		/*
		 * New password passed triviality check.  Reset
		 * insist to 0 in case user does not retype password
		 * correctly and we have to loop all over.
		 */
		insist = 0;

		/* Now make sure user re-types in passwd correctly. */
		if (try_first_pass || use_first_pass) {
			/*
			 * Try the new password entered to the first password
			 * module in the stack (try/use_first_pass option).
			 */
			if ((retcode = __pam_get_authtok(pamh, PAM_HANDLE,
				PAM_AUTHTOK, PASSWORD_LEN, NULL,
				&pswd, NULL)) != PAM_SUCCESS) {
					/* clear bad "new password" if saved */
					if (passwd_already_set == 0)
						pam_set_item(pamh,
							PAM_AUTHTOK, NULL);
					return (retcode);
			}
		} else {
			if ((retcode = __pam_get_authtok(pamh,
				PAM_PROMPT, 0, PASSWORD_LEN,
				PAM_MSG(pamh, 20, "Re-enter new password: "),
				&pswd, NULL)) != PAM_SUCCESS) {
					/* clear bad "new password" if saved */
					if (passwd_already_set == 0)
						pam_set_item(pamh,
							PAM_AUTHTOK, NULL);
					return (retcode);
			}
		}

		if ((strlen(pswd) != strlen(pwbuf)) ||
		    (strncmp(pswd, pwbuf, strlen(pwbuf)))) {
			memset(pswd, 0, strlen(pswd));
			free(pswd);
			if (++count >= MAX_CHANCES) {
				(void) snprintf(messages[0],
					sizeof (messages[0]),
					PAM_MSG(pamh, 21,
				    "%s%s: Too many tries; try again later"),
					prognamep, LDAP_MSG);
				(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
					1, messages, NULL);
				/* clear bad "new password" if saved */
				if (passwd_already_set == 0)
					pam_set_item(pamh, PAM_AUTHTOK, NULL);
				return (PAM_PERM_DENIED);
			} else {
				(void) snprintf(messages[0],
						sizeof (messages[0]),
						PAM_MSG(pamh, 22,
				"%s%s: They don't match; try again."),
						prognamep, LDAP_MSG);
				(void) __pam_display_msg(
						pamh, PAM_ERROR_MSG, 1,
						messages, NULL);
			}
			continue;
		}

		/* password matched - exit loop and return PAM_SUCCESS */
		done = 1;
	} /* while loop */

	/*
	 * If we exit the while loop with too
	 * many attempts return error.
	 */
	if (insist >= MAX_CHANCES) {
		if (debug)
			syslog(LOG_DEBUG,
			"get_newpasswd: failed trivial check");
		(void) snprintf(messages[0],
				sizeof (messages[0]),
				PAM_MSG(pamh, 23,
				"%s%s: Too many failures - try later."),
				prognamep, LDAP_MSG);
		(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
			1, messages, NULL);
		/* clear bad "new password" if saved */
		if (passwd_already_set == 0)
			pam_set_item(pamh, PAM_AUTHTOK, NULL);
		return (PAM_PERM_DENIED);
	}

	/* new password passed triviality check and re-type check */
	return (PAM_SUCCESS);
}

/*
 * triviality_checks():
 *	Checks for the UNIX rules regarding password such as minimum
 *	length etc. For now, the same rules governing UNIX passwords is
 *	enforced in LDAP. This will change later on.
 */
static int
triviality_checks(
	pam_handle_t *pamh,
	uid_t uid,
	char *opwbuf,
	char *pwbuf,
	int nowarn)
{

	int	retcode = 0;
	int	tmpflag = 0;
	int	flags = 0;
	char	*p, *o;
	int	c;
	int 	i, j, k;
	int	bare_minima = MINLENGTH;
	char	*char_p;
	char	*usrname, *prognamep;
	char 	messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];
	int pwlen = strlen(pwbuf);

#ifdef DEBUG
	fprintf(stderr, "\n[update_password.c]\n");
	fprintf(stderr, "\ttriviality_checks()\n");
#endif

	if ((retcode = pam_get_item(pamh, PAM_USER, (void **)&usrname))
							!= PAM_SUCCESS ||
	    (retcode = pam_get_item(pamh, PAM_SERVICE, (void **)&prognamep))
							!= PAM_SUCCESS)
		return (retcode);

	/*
	 * Make sure new password is long enough
	 */

	/*
	 * Open "/etc/default/passwd" file,
	 * if password administration file can't be opened
	 * use built in defaults.
	 */
	if ((defopen(PWADMIN)) == 0) {
		/* get minimum length of password */
		if ((char_p = defread("PASSLENGTH=")) != NULL)
			bare_minima = atoi(char_p);

		/* close defaults file */
		defopen(NULL);
	}
	if (bare_minima < MINLENGTH)
		bare_minima = MINLENGTH;
	else if (bare_minima > MAXLENGTH)
		bare_minima = MAXLENGTH;

	if (pwlen < bare_minima) {
		if (!nowarn) {
			(void) snprintf(messages[0],
					sizeof (messages[0]),
					PAM_MSG(pamh, 24,
		"%s%s: Password too short - must be at least %d characters."),
					prognamep, LDAP_MSG, bare_minima);
			retcode = __pam_display_msg(pamh, PAM_ERROR_MSG,
				1, messages, NULL);
			if (retcode != PAM_SUCCESS) {
				return (retcode);
			}
		}
		return (PAM_AUTHTOK_ERR);
	}

	/* Check the circular shift of the logonid */

	if (circ(usrname, pwbuf)) {
		if (!nowarn) {
			(void) snprintf(messages[0],
					sizeof (messages[0]),
					PAM_MSG(pamh, 25,
		"%s%s: Password cannot be circular shift of logonid."),
				prognamep, LDAP_MSG);
			retcode = __pam_display_msg(pamh, PAM_ERROR_MSG,
				1, messages, NULL);
			if (retcode != PAM_SUCCESS) {
				return (retcode);
			}
		}
		return (PAM_AUTHTOK_ERR);
	}

	/*
	 * Ensure passwords contain at least two alpha characters
	 * and one numeric or special character
	 */

	flags = 0;
	tmpflag = 0;
	p = pwbuf;
	c = *p++;
	while (c != '\0') {
		if (isalpha(c) && tmpflag)
			flags |= 1;
		else if (isalpha(c) && !tmpflag) {
			flags |= 2;
			tmpflag = 1;
		} else if (isdigit(c))
			flags |= 4;
		else
			flags |= 8;
		c = *p++;
	}

	/*
	 * 7 = lca, lca, num
	 * 7 = lca, uca, num
	 * 7 = uca, uca, num
	 * 11 = lca, lca, spec
	 * 11 = lca, uca, spec
	 * 11 = uca, uca, spec
	 * 15 = spec, num, alpha, alpha
	 */

	if (flags != 7 && flags != 11 && flags != 15) {
		if (!nowarn) {
		    (void) snprintf(messages[0],
				sizeof (messages[0]),
				PAM_MSG(pamh, 26,
	"%s%s: The first %d characters of the password"),
				prognamep, LDAP_MSG, bare_minima);
		    (void) snprintf(messages[1],
				sizeof (messages[1]),
				PAM_MSG(pamh, 27,
	"must contain at least two alphabetic characters and at least"));
		    (void) snprintf(messages[2],
				sizeof (messages[2]),
				PAM_MSG(pamh, 28,
	"one numeric or special character."));
		    retcode = __pam_display_msg(pamh,
			PAM_ERROR_MSG, 3, messages, NULL);
		    if (retcode != PAM_SUCCESS) {
			return (retcode);
		    }
		}
		return (PAM_AUTHTOK_ERR);
	}

	p = pwbuf;
	o = opwbuf;
	if (pwlen >= strlen(opwbuf)) {
		i = pwlen;
		k = pwlen - strlen(opwbuf);
	} else {
		i = strlen(opwbuf);
		k = strlen(opwbuf) - pwlen;
	}
	for (j = 1; j <= i; j++)
		if (*p++ != *o++)
			k++;
	if (k  <  3) {
		if (!nowarn) {
		    (void) snprintf(messages[0],
				sizeof (messages[0]),
				PAM_MSG(pamh, 29,
		"%s%s: Passwords must differ by at least 3 positions"),
				prognamep, LDAP_MSG);
		    retcode = __pam_display_msg(pamh,
			PAM_ERROR_MSG, 1, messages, NULL);
		    if (retcode != PAM_SUCCESS) {
			return (retcode);
		    }
		}
		return (PAM_AUTHTOK_ERR);
	}

	return (PAM_SUCCESS);
}

/*
 * circ():
 * 	This function return 1 if string "t" is a circular shift of
 *	string "s", else it returns 0.
 */

static int
circ(
	char *s,
	char *t)
{
	char c, *p, *o, *r, buff[25], ubuff[25], pubuff[25];
	int i, j, k, l, m;

	m = 2;
	i = strlen(s);
	o = &ubuff[0];
	for (p = s; c = *p++; *o++ = c)
		if (islower(c))
			c = toupper(c);
	*o = '\0';
	o = &pubuff[0];
	for (p = t; c = *p++; *o++ = c)
		if (islower(c))
			c = toupper(c);

	*o = '\0';

	p = &ubuff[0];
	while (m--) {
		for (k = 0; k  <=  i; k++) {
			c = *p++;
			o = p;
			l = i;
			r = &buff[0];
			while (--l)
				*r++ = *o++;
			*r++ = c;
			*r = '\0';
			p = &buff[0];
			if (strcmp(p, pubuff) == 0)
				return (1);
		}
		p = p + i;
		r = &ubuff[0];
		j = i;
		while (j--)
			*--p = *r++;
	}
	return (0);
}


/*
 * change_password():
 * 	To update the password in ldap..
 */

static int
change_password(
	pam_handle_t 	*pamh,
	char		*oldpwd,	/* old passwd: clear */
	char		*newpwd,	/* new passwd: clear */
	struct passwd	*ldap_pwd,	/* password structure */
	int		debug,
	int		nowarn)
{
	char 		*prognamep;
	char 		*usrname;
	int		retcode;
	int		ldaprc;
	char		*dn;
	char		buffer[BUFSIZ];
	ns_ldap_error_t	*errorp = NULL;
	char		**newattr = NULL;
	char 		messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];
	Auth_t		*authp = NULL; /* cred struct used in libsldap calls */
	ns_ldap_attr_t	*attrs[2];

#ifdef DEBUG
	fprintf(stderr, "\n[update_password.c]\n");
	fprintf(stderr, "\tchange_password()\n");
#endif

	if ((retcode = pam_get_item(pamh, PAM_SERVICE, (void **)&prognamep))
				!= PAM_SUCCESS ||
	    (retcode = pam_get_item(pamh, PAM_USER, (void **)&usrname))
				!= PAM_SUCCESS)
		goto out;


	if (debug) {
		syslog(LOG_DEBUG,
		    "change_password(): update passwords");
		syslog(LOG_DEBUG,
		    "change_password: username = %s", usrname);
	}

	/* First try to authenticate before updating the password */
	if ((retcode = authenticate(&authp, usrname, oldpwd)) != PAM_SUCCESS)
		goto out;

	if (ldap_pwd == NULL) {
		if (!nowarn) {
			pam_get_item(pamh, PAM_USER, (void **)&usrname);
			snprintf(messages[0],
			    sizeof (messages[0]),
			    PAM_MSG(pamh, 17,
			    "System error: no LDAP passwd record for %s"),
			    usrname);
			(void) __pam_display_msg(pamh,
			    PAM_ERROR_MSG, 1, messages, NULL);
		}
		retcode = PAM_USER_UNKNOWN;
		goto out;
	}

	/* Convert the user name to a distinguished name */
	ldaprc = __ns_ldap_uid2dn(ldap_pwd->pw_name, NULL,
					&dn, NULL, &errorp);

	if (retcode = __ldap_to_pamerror(ldaprc) != PAM_SUCCESS)
		goto out;

	/* newattr is assigned the new value of the password */
	newattr = (char **)calloc(2, sizeof (char *));
	if (newattr == NULL) {
		retcode = -1;
		goto out;

	}
	newattr[1] = NULL;

	strcpy(buffer, newpwd);
	newattr[0] = buffer;
	if ((attrs[0] = (ns_ldap_attr_t *)calloc(1,
				sizeof (ns_ldap_attr_t))) == NULL) {
		retcode = -1;
		goto out;
	}
	attrs[0]->attrname = PASS_ATTR;
	attrs[0]->attrvalue = newattr;
	attrs[0]->value_count = 1;
	attrs[1] = NULL;

	/* Change password */
	ldaprc = __ns_ldap_repAttr(dn, (const ns_ldap_attr_t * const *)attrs,
					authp, 0, &errorp);

	retcode = __ldap_to_pamerror(ldaprc);

out:
	if (dn)
		free(dn);
	if (newattr)
		free(newattr);
	if (attrs[0])
		free(attrs[0]);

	if (errorp)
		__ns_ldap_freeError(&errorp);

	if (retcode != PAM_SUCCESS) {
	/* Printing appropriate messages */
		snprintf(messages[0],
		    sizeof (messages[0]),
		    PAM_MSG(pamh, 18,
		    "%s %s: Couldn't change passwd for %s"),
		    prognamep, LDAP_MSG, usrname);
		(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
		    1, messages, NULL);
	} else {
		snprintf(messages[0],
		    sizeof (messages[0]),
		    PAM_MSG(pamh, 19,
		    "LDAP passwd changed for %s"), usrname);
		(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
		    1, messages, NULL);
	}

	__ns_ldap_freeAuth(&authp);

	return (retcode);
}
