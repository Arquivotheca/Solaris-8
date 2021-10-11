/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_enterprise.cc	1.12	96/04/05 SMI"


#include <synch.h>
#include <sys/systeminfo.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/nis.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>
#include <xfn/xfn.hh>
#include <xfn/fnselect.hh>
#include "FNSP_enterprise_nisplus.hh"
#include "FNSP_enterprise_nis.hh"
#include "FNSP_enterprise_files.hh"

// An array for different name service (NIS+, NIS and /etc files)
// Make the array size 4, so that we can directly index with
// user_specified_ns. Use "0" index for unspecified name service
static FNSP_enterprise *FNSP_real_enterprise[4] = {0, 0, 0, 0};
static mutex_t real_enterprise_lock = DEFAULTMUTEX;


// real_enterprise_lock should be held by callers of FNSP_set_real_enterprise.
//
static FNSP_enterprise*
FNSP_set_real_enterprise(int ns)
{
	char	domain[NIS_MAXNAMELEN+1];

	mutex_lock(&real_enterprise_lock);
	if (FNSP_real_enterprise[ns]) {
		mutex_unlock(&real_enterprise_lock);
		return (FNSP_real_enterprise[ns]);
	}

	switch (ns) {
	case FNSP_nisplus_ns:
		FNSP_real_enterprise[ns]
		    = new FNSP_enterprise_nisplus();
		break;
	case FNSP_nis_ns:
		sysinfo(SI_SRPC_DOMAIN, domain, NIS_MAXNAMELEN);
		FNSP_real_enterprise[ns] = new
			FNSP_enterprise_nis((unsigned char *)domain);
		break;
	case FNSP_files_ns:
		FNSP_real_enterprise[ns] = new
			FNSP_enterprise_files;
		break;
	default:
		ns = 0;
		break;
	}
	mutex_unlock(&real_enterprise_lock);
	return (FNSP_real_enterprise[ns]);
}

FNSP_enterprise*
FNSP_get_enterprise(int ns)
{
	return (FNSP_set_real_enterprise(ns));
}

const FN_string *
FNSP_enterprise::get_root_orgunit_name()
{
	return (root_directory);
}

FN_string*
FNSP_enterprise::get_user_orgunit_name(uid_t,
    const FNSP_enterprise_user_info *,
    FN_string **shortform)
{
	FN_string *orgunit = new FN_string(*root_directory);
	if (shortform)
		*shortform = NULL;
	return (orgunit);
}

FN_string*
FNSP_enterprise::get_user_name(uid_t target_uid,
    const FNSP_enterprise_user_info * /* einfo */)
{
	const int max_username_len = 64;
	char username[max_username_len];
	struct passwd pw;
	const int passwd_buf_size = 256;
	char buffer[passwd_buf_size];
	if ((getpwuid_r(target_uid, &pw,
	    buffer, passwd_buf_size) != NULL) && (pw.pw_name != NULL))
		strcpy(username, pw.pw_name);
	else
		username[0] = '\0';

	if (username[0] == '\0')
		return (0);
	else
		return (new FN_string((unsigned char *) username));
}

FN_string*
FNSP_enterprise::get_host_orgunit_name(FN_string **shortform)
{
	FN_string *orgunit = new FN_string(*root_directory);
	if (shortform)
		*shortform = NULL;
	return (orgunit);
}

FN_string*
FNSP_enterprise::get_host_name()
{
	const int max_hostname_len = 257;
	char hostname[max_hostname_len];

	if (sysinfo(SI_HOSTNAME, hostname, max_hostname_len) > 0)
		return (new FN_string((unsigned char *) hostname));
	else
		return (0);
}
