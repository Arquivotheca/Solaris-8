/*
 * Copyright (c) 1992-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)smartcard_open_session.c	1.3	99/05/18 SMI"

/*
 * pam_sm_open_session 	- session management for individual card users
 */

#include "smartcard_headers.h"

/*ARGSUSED*/
int
pam_sm_open_session(
	pam_handle_t	*pamh,
	int	flags,
	int	argc,
	const char	**argv)
{
	int	error;
	char	*ttyn, *rhost, *user;
	int	fdl;
	struct lastlog	newll;
	struct passwd pwd;
	char	buffer[2048];
	int	i;
	int	debug = 0;
	offset_t	offset;
	time_t	cur_time;

	for (i = 0; i < argc; i++) {
		if (strcasecmp(argv[i], "debug") == 0)
			debug = 1;
		else
			syslog(LOG_ERR, "illegal option %s", argv[i]);
	}

	if ((error = pam_get_item(pamh, PAM_TTY, (void **)&ttyn))
							!= PAM_SUCCESS ||
	    (error = pam_get_item(pamh, PAM_USER, (void **)&user))
							!= PAM_SUCCESS ||
	    (error = pam_get_item(pamh, PAM_RHOST, (void **)&rhost))
							!= PAM_SUCCESS) {
		return (error);
	}

	if (getpwnam_r(user, &pwd, buffer, sizeof (buffer)) == NULL) {
		return (PAM_USER_UNKNOWN);
	}

	if ((fdl = open(LASTLOG, O_RDWR|O_CREAT, 0444)) >= 0) {

		/*
		 * The value of lastlog is read by the 
		 * account management module
		 */

		offset = (offset_t) pwd.pw_uid *
					(offset_t) sizeof (struct lastlog);

		if (llseek(fdl, offset, SEEK_SET) != offset) {
			/*
			 * XXX uid too large for database
			 */
			return (PAM_SUCCESS);
		}

		/*
		 * use time32_t in case of _LP64
		 * since it's written in lastlog.h
		 */
		(void) time(&cur_time);
#ifdef _LP64
		newll.ll_time = (time32_t) cur_time;
#else
		newll.ll_time = cur_time;
#endif
		strncpy(newll.ll_line,
			(ttyn + sizeof ("/dev/")-1),
			sizeof (newll.ll_line));
		strncpy(newll.ll_host, rhost, sizeof (newll.ll_host));

		(void) write(fdl, (char *)&newll, sizeof (newll));
		(void) close(fdl);
	}

	return (PAM_SUCCESS);
}
