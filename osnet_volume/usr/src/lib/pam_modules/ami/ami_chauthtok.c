/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)ami_chauthtok.c	1.1 99/07/11 SMI"
 */

#include <strings.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include "ami_auth_headers.h"

static int retrieve_old_password(pam_handle_t *, char **, int, int, int, int);
static int retrieve_new_password(pam_handle_t *, char *, char **,
				int, int, int, int);
static int triviality_checks(pam_handle_t *, char *oldpw, char *newpw);

int
pam_sm_chauthtok(
	pam_handle_t *pamh,
	int   flags,
	int     argc,
	const char **argv)
{
	int	debug = 0;
	int     try_first_pass = 0;
	int     use_first_pass = 0;
	int	nowarn = 0;
	int     i;
	int	retcode;
	char	*prognamep;
	char	*userName;
	long	uid;
	char	*oldpw;
	char	*newpw;
	struct  passwd pd;
	char    pwd_buf[1024];
	char    messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];

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
				"AMI chauthtok(): illegal module option %s",
				argv[i]);
	}

	if (flags & PAM_PRELIM_CHECK) {
		if (debug)
			syslog(LOG_DEBUG,
			       "ami pam_sm_chauthtok(): prelim check");
		return (PAM_SUCCESS);
	}

	/* make sure PAM framework is telling us to update passwords */
	if (!(flags & PAM_UPDATE_AUTHTOK)) {
		syslog(LOG_ERR, "ami pam_sm_chauthtok: bad flags: %d", flags);
		return (PAM_SYSTEM_ERR);
	}

	/* TODO: what shall we do here? */
	/* if (flags & PAM_CHANGE_EXPIRED_AUTHTOK) {
	} */

	if (debug) {
		syslog(LOG_DEBUG, "ami pam_sm_chauthtok(): update passwords");	
		syslog(LOG_DEBUG, "ami pam_sm_chauthtok: uid = %d, euid = %d",
			getuid(), geteuid());
	}

	if ((retcode = pam_get_item(pamh, PAM_SERVICE, (void **)&prognamep))
					!= PAM_SUCCESS ||
	    (retcode = pam_get_item(pamh, PAM_USER, (void **)&userName))
					!= PAM_SUCCESS) {
		goto out;
	}

	/*
	 * TODO: check out whether the old_authtok and authtok have been
	 *       set in the pam handle. If not, we should clear BAD passwords
	 *       before we return.
	 */

	if (getpwnam_r(userName, &pd, pwd_buf, sizeof (pwd_buf))
			== NULL)
		return (PAM_USER_UNKNOWN);

	
	if ((strcmp(prognamep, "passwd") == 0)) {
		uid = getuid();
		/*
		 * Even a privileged user cannot change another user's
		 * AMI password.
		 */
		if (uid != pd.pw_uid) {
			snprintf(messages[0],
				sizeof (messages[0]),
				" ");
			snprintf(messages[1],
				sizeof (messages[1]),
				PAM_MSG(pamh, 4,
 		"A user cannot change AMI password for another user"));
			(void) __pam_display_msg(pamh, PAM_ERROR_MSG, 2,
							messages, NULL);
			return (PAM_PERM_DENIED);
		}
	} else {
		/*
		 * Service is login, rlogin, etc.
		 */
		uid = pd.pw_uid;
	}

	/*
	 * If try_first_pass is set, we may go through retrieve_old_password,
	 * retrieve_new_password, and ami_attempt_chauthtok twice:
	 * the first time, try to get password from PAM handle; if those
	 * password are bad for AMI module, we will get passwords from
	 * prompts at the second time.
	 */
try:
	if (retrieve_old_password(pamh, &oldpw, try_first_pass,
			use_first_pass, nowarn, debug)) {
		retcode = PAM_AUTHTOK_ERR;
		goto out;
	}
	
	if (retrieve_new_password(pamh, oldpw, &newpw, try_first_pass,
			use_first_pass,  nowarn, debug)) {
		retcode = PAM_AUTHTOK_RECOVERY_ERR;
		goto out;
	}

	if (ami_attempt_chauthtok(uid, userName, oldpw, newpw, flags, debug)
			!= PAM_SUCCESS) {
		if (try_first_pass) {
			try_first_pass = 0;
			goto try;
		}
		retcode = PAM_AUTHTOK_ERR;
		goto out;
	}

	retcode = PAM_SUCCESS;

out:
	/* clean up */
	/*
	 * TODO: if this AMI module sets old_authtok and authtok in
	 * pam handle, and they are BAD passwords, we should clear
	 * BAD passwords in pam handle before we return.
	 *
	 * TODO: In case AMI pam_sm_chauthtok() fails, we should
	 * display an error message: AMI password change failed.
	 */
	if (oldpw) {
		memset(oldpw, 0, strlen(oldpw));
		free(oldpw);
	}
	if (newpw) {
		memset(newpw, 0, strlen(newpw));
		free(newpw);
	}

	if (retcode == PAM_SUCCESS) {
		snprintf(messages[0],
			sizeof (messages[0]),
			PAM_MSG(pamh, 5,
			"%s (AMI): passwd successfully changed for %s"),
			prognamep, userName);
		(void) __pam_display_msg(pamh, PAM_TEXT_INFO,
				1, messages, NULL);
	}
		
	return (retcode);
}

static int retrieve_old_password(pam_handle_t *pamh,
		char **oldpw,
		int try_first_pass,
		int use_first_pass,
		int nowarn,
		int debug) 
{
	int retcode = PAM_SYSTEM_ERR;
	char prompt[PAM_MAX_MSG_SIZE];

	if (pamh == NULL || oldpw == NULL)
		goto out;

	*oldpw = NULL;

	if (try_first_pass || use_first_pass) {
		retcode = __pam_get_authtok(pamh, PAM_HANDLE,
				PAM_OLDAUTHTOK, PASSWORD_LEN, 0,
				oldpw, NULL);
		goto out;
	}
			
	memset(prompt, 0, PAM_MAX_MSG_SIZE);
	strcpy(prompt, 
		PAM_MSG(pamh, 1, "Enter old AMI password:"));
	retcode = __pam_get_authtok(pamh, PAM_PROMPT,
			PAM_OLDAUTHTOK, PASSWORD_LEN,
			prompt, oldpw, NULL);
out:
	return (retcode);
}

static int
retrieve_new_password(pam_handle_t *pamh, char *oldpw, char **newpw,
	int try_first_pass, int use_first_pass, int nowarn, int debug)
{
	int retcode = PAM_SYSTEM_ERR;
	char *vnewpw = NULL;

	if (pamh == NULL || oldpw == NULL)
		goto out;

	if (try_first_pass || use_first_pass) {
		if ((retcode = __pam_get_authtok(pamh,
					PAM_HANDLE,
					PAM_AUTHTOK,
					PASSWORD_LEN,
					NULL,
					newpw,
					NULL)) != PAM_SUCCESS)
			goto out;

		/* see if new passwd passes the test */
		retcode = triviality_checks(pamh, oldpw, *newpw);
		goto out;	
	}

prompt:
	if ((retcode = __pam_get_authtok(pamh,
				PAM_PROMPT,
				PAM_AUTHTOK,
			 	PASSWORD_LEN,
				PAM_MSG(pamh, 2, "New AMI password: "),
				newpw, NULL)) != PAM_SUCCESS) {
		goto out;	
	}	

	if (*newpw == NULL || *newpw[0] == '\0') {
		/* Need a password to proceed */
		retcode = PAM_AUTHTOK_ERR;
		goto out;
	}

	if ((retcode = __pam_get_authtok(pamh,
				PAM_PROMPT,
				0,
				PASSWORD_LEN,
				PAM_MSG(pamh, 3, "Re-enter new AMI password: "),
				&vnewpw, NULL)) != PAM_SUCCESS) {
		goto out;
	}

	if (strcmp(*newpw, vnewpw)) {
		retcode = PAM_AUTHTOK_ERR;
		goto out;
	} else {
		retcode = PAM_SUCCESS;
	}

	/*
	 * Check whether the new password is acceptable.
	 */
	retcode = triviality_checks(pamh, oldpw, *newpw);
		
out:
	if (vnewpw)
		free(vnewpw);

	return(retcode);
}

static int
triviality_checks(pam_handle_t *pamh, char *oldpw, char *newpw)
{
	if (oldpw == NULL || newpw == NULL)
		return (PAM_SYSTEM_ERR);

	/* make sure passwords are not the same */
	if (strncmp(oldpw, newpw, strlen(oldpw)) == 0) {
		return (PAM_AUTHTOK_RECOVERY_ERR);
	}

	return (PAM_SUCCESS);
}
