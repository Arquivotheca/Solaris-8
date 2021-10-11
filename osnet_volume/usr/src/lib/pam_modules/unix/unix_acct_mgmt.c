/*
 * Copyright (c) 1992-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)unix_acct_mgmt.c	1.13	99/07/14 SMI"

#include <deflt.h>	/* for defopen, defread */
#include "unix_headers.h"

/*ARGSUSED*/
static void
unix_cleanup(
	pam_handle_t *pamh,
	void *data,
	int pam_status)
{
	free((unix_authtok_data *)data);
}

/*
 * check_for_login_inactivity	- Check for login inactivity
 *
 */

static int
check_for_login_inactivity(
	struct 	passwd 	*pwd,
	struct 	spwd 	*shpwd)
{
	int		fdl;
	struct lastlog	ll;
	int		retval;
	offset_t	offset;

	offset = (offset_t) pwd->pw_uid * (offset_t) sizeof (struct lastlog);

	if ((fdl = open(LASTLOG, O_RDWR|O_CREAT, 0444)) >= 0) {
		/*
		 * Read the last login (ll) time
		 */
		if (llseek(fdl, offset, SEEK_SET) != offset) {
			/*
			 * XXX uid too large for database
			 */
			return (0);
		}

		retval = read(fdl, (char *)&ll, sizeof (ll));

		/* Check for login inactivity */

		if ((shpwd->sp_inact > 0) && (retval == sizeof (ll)) &&
		    ll.ll_time) {
			if ((time_t)((ll.ll_time / DAY) + shpwd->sp_inact)
				< DAY_NOW) {
				/*
				 * Account inactive for too long
				 */
				(void) close(fdl);
				return (1);
			}
		}

		(void) close(fdl);
	}
	return (0);
}

/*
 * new_password_check()
 *
 * check to see if the user needs to change their password
 */

static int
new_password_check(pwd, shpwd, flags)
	struct 	passwd 	*pwd;
	struct 	spwd 	*shpwd;
	int 	flags;
{
	int	status = PAM_SYSTEM_ERR;
	time_t	now  = DAY_NOW;
	int	max = -1;
	int	min = -1;
	int	lastchg = -1;
	char	*aging_value = 0;

	/*
	 * We want to make sure that we change the password only if
	 * passwords are required for the system, the user does not
	 * have a password, AND the user's NULL password can be changed
	 * according to its password aging information
	 */

	if ((flags & PAM_DISALLOW_NULL_AUTHTOK) != 0) {

		/* check for old SunOS password aging */
		if (shpwd->sp_pwdp[0] &&
		    (aging_value = strchr(shpwd->sp_pwdp, ','))) {

			/* get the SunOS aging values in days */
			if ((status = decode_passwd_aging
					(aging_value,
					&max,
					&min,
					&lastchg)) != PAM_SUCCESS) {
				return (status);
			}

			if (max == 0 && min == 0)
				return (PAM_NEW_AUTHTOK_REQD);

		} else if (shpwd->sp_pwdp[0] == '\0') {
			if ((pwd->pw_uid != 0) &&
				((shpwd->sp_max == -1) ||
				((time_t)shpwd->sp_lstchg > now) ||
				((now >= (time_t)(shpwd->sp_lstchg +
							shpwd->sp_min)) &&
				(shpwd->sp_max >= shpwd->sp_min)))) {
					return (PAM_NEW_AUTHTOK_REQD);
			}
		}
	}
	return (PAM_SUCCESS);
}

/*
 * perform_passwd_aging_check
 *		- Check for password exipration.
 */

static	int
perform_passwd_aging_check(
	pam_handle_t *pamh,
	struct 	spwd 	*shpwd,
	int	flags)
{
	int	status = PAM_SYSTEM_ERR;
	time_t 	now = DAY_NOW;
	int	Idleweeks = -1;
	char	*ptr;
	char	*aging_value = 0;
	int	max = -1, min = -1, lastchg = -1;
	char	messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];


	if (defopen(LOGINADMIN) == 0) {
		if ((ptr = defread("IDLEWEEKS=")) != NULL)
			Idleweeks = atoi(ptr);
		(void) defopen(NULL);
	}

	/* check for old SunOS password aging */
	if (shpwd->sp_pwdp[0] &&
	    (aging_value = strchr(shpwd->sp_pwdp, ','))) {

		/* get the SunOS aging values in days */
		if ((status = decode_passwd_aging
				(aging_value,
				&max,
				&min,
				&lastchg)) != PAM_SUCCESS) {
			return (status);
		}

		if (lastchg == 0 ||
		    (max > 0 && now > (time_t)(lastchg + max) && max > min))
			return (PAM_NEW_AUTHTOK_REQD);

		return (PAM_SUCCESS);
	}

	if ((shpwd->sp_lstchg == 0) ||
	    ((shpwd->sp_max >= 0) &&
	    (now > (time_t)(shpwd->sp_lstchg + shpwd->sp_max)) &&
	    (shpwd->sp_max >= shpwd->sp_min))) {
		if ((Idleweeks == 0) ||
		    ((Idleweeks > 0) &&
		    (now > (time_t)(shpwd->sp_lstchg + (7 * Idleweeks))))) {
			if (!(flags & PAM_SILENT)) {
			    strcpy(messages[0], PAM_MSG(pamh, 20,
			    "Your password has been expired for too long."));
			    strcpy(messages[1], PAM_MSG(pamh, 21,
			    "Please contact the system administrator"));
			    __pam_display_msg(pamh, PAM_ERROR_MSG,
				2, messages, NULL);
			}
			return (PAM_AUTHTOK_EXPIRED);
		} else {
			return (PAM_NEW_AUTHTOK_REQD);
		}
	}
	return (PAM_SUCCESS);
}

/*
 * warn_user_passwd_will_expire	- warn the user when the password will
 *					  expire.
 */

static void
warn_user_passwd_will_expire(
	pam_handle_t *pamh,
	struct 	spwd shpwd)
{
	time_t 	now	= DAY_NOW;
	char	messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];
	time_t	days;

	/* ignore if using SunOS password aging */
	if (shpwd.sp_pwdp && strchr(shpwd.sp_pwdp, ','))
		return;

	if ((shpwd.sp_warn > 0) && (shpwd.sp_max > 0) &&
	    (now + shpwd.sp_warn) >= (time_t)(shpwd.sp_lstchg + shpwd.sp_max)) {
		days = (time_t)(shpwd.sp_lstchg + shpwd.sp_max) - now;
		if (days <= 0)
			snprintf(messages[0],
				sizeof (messages[0]),
				PAM_MSG(pamh, 22,
				"Your password will expire within 24 hours."));
		else if (days == 1)
			snprintf(messages[0],
				sizeof (messages[0]),
				PAM_MSG(pamh, 23,
				"Your password will expire in %d day."),
				(int)days);
		else
			snprintf(messages[0],
				sizeof (messages[0]),
				PAM_MSG(pamh, 24,
				"Your password will expire in %d days."),
				(int)days);

		__pam_display_msg(pamh, PAM_TEXT_INFO, 1, messages, NULL);
	}
}

/*
 * pam_sm_acct_mgmt	- 	main account managment routine.
 *			  Returns: module error or specific error on failure
 */

int
pam_sm_acct_mgmt(
	pam_handle_t	*pamh,
	int	flags,
	int	argc,
	const char	**argv)
{
	struct 	spwd shpwd;
	struct 	passwd pwd;
	char	pwd_buf[1024];
	char	shpwd_buf[1024];
	int 	error = PAM_ACCT_EXPIRED;
	char    *user;
	int	i;
	int	debug = 0;
	int	nowarn = 0;
	unix_authtok_data *status;

	for (i = 0; i < argc; i++) {
		if (strcasecmp(argv[i], "debug") == 0)
			debug = 1;
		else if (strcasecmp(argv[i], "nowarn") == 0) {
			nowarn = 1;
			flags = flags | PAM_SILENT;
		} else {
			syslog(LOG_ERR,
				"UNIX pam_sm_acct_mgmt: illegal option %s",
				argv[i]);
		}
	}

	if ((error = pam_get_item(pamh, PAM_USER, (void **)&user))
							!= PAM_SUCCESS)
		goto out;

	if (user == NULL) {
		error = PAM_USER_UNKNOWN;
		goto out;
	}

	/*
	 * Get the password and shadow password entries
	 */
	if (getpwnam_r(user, &pwd, pwd_buf, sizeof (pwd_buf)) == NULL ||
	    getspnam_r(user, &shpwd, shpwd_buf, sizeof (shpwd_buf)) == NULL) {
		error = PAM_USER_UNKNOWN;
		goto out;
	}

	/*
	 * Check for account expiration
	 */
	if (shpwd.sp_expire > 0 &&
	    (time_t)shpwd.sp_expire < DAY_NOW) {
		error = PAM_ACCT_EXPIRED;
		goto out;
	}

	/*
	 * Check for excessive login account inactivity
	 */
	if (check_for_login_inactivity(&pwd, &shpwd)) {
		error = PAM_PERM_DENIED;
		goto out;
	}

	/*
	 * Check to see if the user needs to change their password
	 */
	if (error = new_password_check(&pwd, &shpwd, flags)) {
		goto out;
	}

	/*
	 * Check to make sure password aging information is okay
	 */
	if ((error = perform_passwd_aging_check(pamh, &shpwd, flags))
							!= PAM_SUCCESS) {
		goto out;
	}

	/*
	 * Finally, warn the user if their password is about to expire.
	 */
	if (!(flags & PAM_SILENT)) {
		warn_user_passwd_will_expire(pamh, shpwd);
	}

	/*
	 * All done, return Success
	 */
	error = PAM_SUCCESS;

out:

	{
		int pam_res;
		unix_authtok_data *authtok_data;

		memset(shpwd_buf, 0, sizeof (shpwd_buf));
		/* store the password aging status in the pam handle */
		pam_res = pam_get_data(
			pamh, UNIX_AUTHTOK_DATA, (const void **)&authtok_data);

		if ((status = (unix_authtok_data *)calloc
			(1, sizeof (unix_authtok_data))) == NULL) {
			return (PAM_BUF_ERR);
		}

		if (pam_res == PAM_SUCCESS)
			memcpy(status, authtok_data,
				sizeof (unix_authtok_data));

		status->age_status = error;
		if (pam_set_data(pamh, UNIX_AUTHTOK_DATA, status, unix_cleanup)
							!= PAM_SUCCESS) {
			free(status);
			return (PAM_SERVICE_ERR);
		}
	}

	return (error);
}
