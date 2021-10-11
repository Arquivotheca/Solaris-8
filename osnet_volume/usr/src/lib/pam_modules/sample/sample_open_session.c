/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)sample_open_session.c	1.5	98/06/14 SMI"

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <syslog.h>

/*ARGSUSED*/
int
pam_sm_open_session(
	pam_handle_t *pamh,
	int	flags,
	int	argc,
	const char **argv)
{
	return (PAM_SUCCESS);
}
