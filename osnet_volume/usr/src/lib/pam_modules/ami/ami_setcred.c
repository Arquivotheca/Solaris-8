/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)ami_setcred.c	1.1 99/07/11 SMI"
 */

#include <security/pam_appl.h>
#include <security/pam_modules.h>

int
pam_sm_setcred(
	pam_handle_t *pamh,
	int   flags,
	int     argc,
	const char **argv)
{
	/*
	 * Keystore has been registered in pam_sm_authenticate().
	 */
	return (PAM_SUCCESS);
}
