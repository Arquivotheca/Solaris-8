/*
 * Copyright (c) 1992-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)smartcard_close_session.c	1.2	99/05/18 SMI"

#include "unix_headers.h"

/*
 * pam_sm_close_session	- Terminate a PAM authenticated session
 */
/*ARGSUSED*/
int
pam_sm_close_session(
	pam_handle_t	*pamh,
	int	flags,
	int	argc,
	const char	**argv)
{
	int	i;
	int	debug = 0;

	for (i = 0; i < argc; i++) {
		if (strcasecmp(argv[i], "debug") == 0)
			debug = 1;
		else
			syslog(LOG_ERR, "illegal option %s", argv[i]);
	}

	return (PAM_SUCCESS);
}
