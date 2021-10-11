/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_enterprise_nisplus.cc	1.8	96/03/31 SMI"

#include <rpcsvc/nis.h>
#include <rpcsvc/nislib.h>
#include <xfn/xfn.h>
#include <synch.h>
#include <unistd.h>
#include <string.h>
// #include <nss_dbdefs.h>		// NSS_BUFLEN_PASSWD
#define	NSS_BUFLEN_PASSWD 1024

#include "FNSP_enterprise_nisplus.hh"

extern FN_string *
FNSP_strip_root_name(const FN_string &name);

extern const FN_string *
FNSP_get_root_name();

FNSP_enterprise_nisplus::FNSP_enterprise_nisplus()
{
	my_address_type = new
	    FN_identifier((const unsigned char *)NISPLUS_ADDRESS_STR);
}

const FN_identifier*
FNSP_enterprise_nisplus::get_addr_type()
{
	return (my_address_type);
}

/* *********** functions to deal with NIS+ root **************** */

const FN_string*
FNSP_enterprise_nisplus::get_root_orgunit_name()
{
	return (FNSP_get_root_name());
}

extern "C" __nis_principal(char *principal, uid_t uid, char *dir);
static char *
__get_nisplus_principal(uid_t uid)
{
	char principal[NIS_MAXNAMELEN];
	int status;

	principal[0] = '\0';
	if (uid == 0) {
		strcpy(principal, nis_local_host());
		return (strdup(principal));
	}
	char *dirname = nis_local_directory();
	if ((dirname == NULL) || (dirname[0] == NULL)) {
		strcpy(principal, "nobody");
		return (strdup(principal));
	}
	switch (status = __nis_principal(principal, uid, dirname)) {
	case NIS_SUCCESS:
	case NIS_S_SUCCESS:
		break;
	default:
		strcpy(principal, "nobody");
	}
	return (strdup(principal));
}


/*
 * functions to deal with user related information
 */
FNSP_enterprise_user_info *
FNSP_enterprise_nisplus::init_user_info(uid_t uid)
{
	// will get principal name for given uid
	char *principal_name = __get_nisplus_principal(uid);
	return ((FNSP_enterprise_user_info *)principal_name);
}

// 'UserOrgUnit' is derived from user's NIS+ principal name.
// If user's principal name does not contain a domain name (e.g. 'nobody'),
// the host's domain is used instead.

FN_string*
FNSP_enterprise_nisplus::get_user_orgunit_name(
	uid_t /* target_uid */,
	const FNSP_enterprise_user_info *einfo,
	FN_string **shortform)
{
	if (shortform)
		*shortform = NULL; // initialize

	char *principal_name = (char *)einfo;
	char *principal_domain = 0;

	if (principal_name) {
		principal_domain = nis_domain_of(principal_name);
		if (principal_domain && *principal_domain == '.')
			principal_domain = 0;  // invalid domain name
	}
	// If cannot determine principal domain, use machine's directory

	if (principal_domain == 0)
		principal_domain = nis_local_directory();

	FN_string *orgunit = new
	    FN_string((unsigned char *)(principal_domain));

	if (shortform && orgunit)
		*shortform = FNSP_strip_root_name(*orgunit);

	return (orgunit);
}

#include <string.h>
#include <pwd.h>
// #include <sys/types.h>

static inline int
is_nobody(const char *pname)
{
	return (strcmp(pname, "nobody") == 0);
}

FN_string*
FNSP_enterprise_nisplus::get_user_name(
	uid_t target_uid, const FNSP_enterprise_user_info *einfo)
{
	char *principal_name = (char *)einfo;
	const int max_username_len = 64;
	char username[max_username_len];
	username[0] = '\0';

	if (principal_name && !is_nobody(principal_name)) {
		nis_leaf_of_r(principal_name, username, max_username_len);
	}

	// could not get user name from principal name (e.g. nobody)
	// extract from passwd entry
	if (username[0] == '\0') {
		struct passwd pw;
		char buffer[NSS_BUFLEN_PASSWD];
		if (getpwuid_r(target_uid, &pw, buffer, NSS_BUFLEN_PASSWD)
		    != NULL && pw.pw_name != NULL) {
			strncpy(username, pw.pw_name, max_username_len - 1);
			username[max_username_len-1] = '\0';
		}
	}
	if (username[0] == '\0')
		return (0);
	else
		return (new FN_string((unsigned char *)username));
}


// ************* functions to determine information related to host ***

FN_string*
FNSP_enterprise_nisplus::get_host_orgunit_name(FN_string **shortform)
{
	char *domainname = nis_local_directory();

	FN_string* hostorgunit_name = new
	    FN_string((unsigned char *)(domainname ? domainname : ""));

	if (shortform) {
		if (domainname)
			*shortform = FNSP_strip_root_name(*hostorgunit_name);
		else
			*shortform = NULL;
	}

	return (hostorgunit_name);
}

FN_string*
FNSP_enterprise_nisplus::get_host_name()
{
	char *nlh = nis_local_host();
	FN_string full_hostname((unsigned char *)(nlh? nlh : ""));

	int first_dot = full_hostname.next_substring((unsigned char *)".");
	FN_string *hostname = new FN_string(full_hostname,
	    FN_STRING_INDEX_FIRST,
	    (first_dot == FN_STRING_INDEX_NONE) ?
	    FN_STRING_INDEX_LAST : first_dot - 1);
	return (hostname);
}
