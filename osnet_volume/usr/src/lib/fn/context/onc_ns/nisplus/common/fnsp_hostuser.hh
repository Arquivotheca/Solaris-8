/*
 * Copyright (c) 1992 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_HOSTUSER_HH
#define	_FNSP_HOSTUSER_HH

#pragma ident	"@(#)fnsp_hostuser.hh	1.2	97/08/15 SMI"

#include <xfn/xfn.hh>
#include <netdb.h>	// for hostent
#include "FNSP_Address.hh"

// user and host name routines
extern FN_string *
FNSP_find_host_entry(const FN_string &directory,
    const FN_string &hostname,
    unsigned int access_flags,
    unsigned &status,
    struct hostent **he = 0);

extern void
FNSP_free_hostent(struct hostent *);

extern FN_string *
FNSP_find_user_entry(const FN_string &directory,
    const FN_string &username,
    unsigned int access_flags,
    unsigned &status);

extern int
FNSP_find_passwd_shadow(const FN_string &dirname,
    const FN_string &username,
    unsigned int access_flags,
    FN_string &passwd,
    FN_string &shadow,
    unsigned int &status);

extern FN_string *
FNSP_nisplus_get_homedir(const FNSP_Address &,
    const FN_string &username,
    unsigned &status);

extern int
FNSP_find_mailentry(const FN_string &dirname,
    const FN_string &username,
    unsigned int access_flags, char *&mailentry,
    unsigned int &status);

#endif /* _FNSP_HOSTUSER_HH */
