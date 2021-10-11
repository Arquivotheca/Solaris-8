/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)ldap_acct_mgmt.c	1.1	99/07/07 SMI"

#include "ldap_headers.h"

/*
 * pam_sm_acct_mgmt	main account managment routine.
 *			This function just returns success
 */

int
pam_sm_acct_mgmt(
	pam_handle_t *pamh,
	int	flags,
	int	argc,
	const char **argv)
{

	return (PAM_SUCCESS);

}
