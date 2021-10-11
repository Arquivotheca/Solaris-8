/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)sc_auth_headers.h	1.2 99/05/18 SMI"
 *
 */

#ifndef _SC_AUTH_HEADERS_H
#define _SC_AUTH_HEADERS_H

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
	(char *) __pam_get_i18n_msg(pamh, "pam_smartcard", 1, number, string)

int
sc_attempt_authentication(long   /* IN */, 
			   char * /* IN */,
			   char * /* IN */,
			   int    /* IN */,
			   int    /* IN */);

int
sc_attempt_chauthtok(long /* IN */,
		char * /* IN */,
		char * /* IN */,
		char * /* IN */,
		int    /* IN */,
		int    /* IN */);


#ifdef __cplusplus
}
#endif

#endif  /* _SC_AUTH_HEADERS_H */
