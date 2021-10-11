/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNCOPY_UTILS_HH
#define	_FNCOPY_UTILS_HH

#pragma ident	"@(#)fncopy_utils.hh	1.2	96/04/10 SMI"

#include <stdio.h>
#include <xfn/xfn.hh>

extern FN_string *
get_user_name_from_ref(FN_ref *);

extern FN_string *
get_host_name_from_ref(FN_ref *);

extern FN_ref *
create_user_fs_ref(const FN_string &user_name,
    const FN_string &domain_name);

extern FN_ref *
create_host_fs_ref(const FN_string &host_name,
    const FN_string &domain_name);

extern void
FNSP_files_change_user_ownership(char *username);

#define	NIS_MAXNAMELEN 256

#endif /* _FNCOPY_UTILS_HH */
