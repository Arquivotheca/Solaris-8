/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)ami_auth_headers.h	1.1 99/07/11 SMI"
 */

#ifndef _AMI_AUTH_HEADERS_H
#define _AMI_AUTH_HEADERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <pam_impl.h>
#include <syslog.h>
#include <pwd.h>

#define PASSWORD_LEN            8

/*
 * PAM_MSG macro for return of internationalized text
 */

#define PAM_MSG(pamh, number, string)\
	(char *) __pam_get_i18n_msg(pamh, "pam_ami", 1, number, string)

int
ami_attempt_authentication(long   /* IN */, 
			   char * /* IN */,
			   char * /* IN */,
			   int    /* IN */,
			   int    /* IN */);

int
ami_attempt_chauthtok(long /* IN */,
		char * /* IN */,
		char * /* IN */,
		char * /* IN */,
		int    /* IN */,
		int    /* IN */);


#ifdef __cplusplus
}
#endif

#endif  /* _AMI_AUTH_HEADERS_H */
