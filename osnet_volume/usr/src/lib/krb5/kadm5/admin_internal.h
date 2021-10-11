#pragma ident	"@(#)admin_internal.h	1.1	99/07/18 SMI"

/*
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING 
 *
 *	Openvision retains the copyright to derivative works of
 *	this source code.  Do *NOT* create a derivative of this
 *	source code before consulting with your legal department.
 *	Do *NOT* integrate *ANY* of this source code into another
 *	product before consulting with your legal department.
 *
 *	For further information, read the top-level Openvision
 *	copyright which is contained in the top-level MIT Kerberos
 *	copyright.
 *
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 */


/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved
 *
 * $Header: /afs/athena.mit.edu/astaff/project/krbdev/.cvsroot/src/lib/kadm5/admin_internal.h,v 1.13 1996/07/25 22:06:39 tytso Exp $
 */

#ifndef __KADM5_ADMIN_INTERNAL_H__
#define __KADM5_ADMIN_INTERNAL_H__

#include <kadm5/admin.h>

#ifdef DEBUG
#define ADMIN_LOG(a, b, c) syslog(a, b, c);
#define ADMIN_LOGO(a, b) syslog(a, b);
#else
#define ADMIN_LOG(a, b, c)
#define ADMIN_LOGO(a, b)
#endif

#define KADM5_SERVER_HANDLE_MAGIC	0x12345800

#define GENERIC_CHECK_HANDLE(handle, old_api_version, new_api_version) \
{ \
	kadm5_server_handle_t srvr = \
	     (kadm5_server_handle_t) handle; \
 \
	if (! srvr) \
		return KADM5_BAD_SERVER_HANDLE; \
	if (srvr->magic_number != KADM5_SERVER_HANDLE_MAGIC) \
		return KADM5_BAD_SERVER_HANDLE; \
	if ((srvr->struct_version & KADM5_MASK_BITS) != \
	    KADM5_STRUCT_VERSION_MASK) \
		return KADM5_BAD_STRUCT_VERSION; \
	if (srvr->struct_version < KADM5_STRUCT_VERSION_1) \
		return KADM5_OLD_STRUCT_VERSION; \
	if (srvr->struct_version > KADM5_STRUCT_VERSION_1) \
		return KADM5_NEW_STRUCT_VERSION; \
	if ((srvr->api_version & KADM5_MASK_BITS) != \
	    KADM5_API_VERSION_MASK) \
		return KADM5_BAD_API_VERSION; \
	if (srvr->api_version < KADM5_API_VERSION_1) \
		return old_api_version; \
	if (srvr->api_version > KADM5_API_VERSION_2) \
		return new_api_version; \
}

/*
 * _KADM5_CHECK_HANDLE calls the function _kadm5_check_handle and
 * returns any non-zero error code that function returns.
 * _kadm5_check_handle, in client_handle.c and server_handle.c, exists
 * in both the server- and client- side libraries.  In each library,
 * it calls CHECK_HANDLE, which is defined by the appropriate
 * _internal.h header file to call GENERIC_CHECK_HANDLE as well as
 * CLIENT_CHECK_HANDLE and SERVER_CHECK_HANDLE.
 *
 * _KADM5_CHECK_HANDLE should be used by a function that needs to
 * check the handle but wants to be the same code in both the client
 * and server library; it makes a function call to the right handle
 * checker.  Code that only exists in one library can call the
 * CHECK_HANDLE macro, which inlines the test instead of making
 * another function call.
 *
 * Got that?
 */
int _kadm5_check_handle();

#define _KADM5_CHECK_HANDLE(handle) \
{ int code; if ((code = _kadm5_check_handle((void *)handle))) return code; }

kadm5_ret_t _kadm5_chpass_principal_util(void *server_handle,
					 void *lhandle,
					 krb5_principal princ,
					 char *new_pw, 
					 char **ret_pw,
					 char *msg_ret);

/* this is needed by the alt_prof code I stole.  The functions
   maybe shouldn't be named krb5_*, but they are. */

krb5_error_code
krb5_string_to_keysalts(char *string, const char *tupleseps,
			const char *ksaltseps, krb5_boolean dups,
			krb5_key_salt_tuple **ksaltp, krb5_int32 *nksaltp);

krb5_error_code
krb5_string_to_flags(char* string, const char* positive, const char* negative,
		     krb5_flags *flagsp);

#endif /* __KADM5_ADMIN_INTERNAL_H__ */
