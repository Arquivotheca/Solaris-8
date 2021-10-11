/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)ldap_close_session.c	1.1	99/07/07 SMI"

#include "ldap_headers.h"

/*ARGSUSED*/
int
pam_sm_close_session(
	pam_handle_t *pamh,
	int	flags,
	int	argc,
	const char **argv)
{
	return (PAM_SUCCESS);
}
