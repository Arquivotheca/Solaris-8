/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_NISPLUS_ROOT_HH
#define	_FNSP_NISPLUS_ROOT_HH

#pragma ident	"@(#)fnsp_nisplus_root.hh	1.1	96/03/31 SMI"

#include <xfn/xfn.hh>

extern int
FNSP_home_hierarchy_p(const FN_string &name);

extern const FN_string *
FNSP_get_root_name();

extern int
FNSP_potential_ancestor_p(const FN_string &name);

extern FN_string *
FNSP_strip_root_name(const FN_string &name);

extern int
FNSP_local_domain_p(const FN_string &orgname);

extern FN_string *
FNSP_short_orgname(const FN_string &orgname);

#endif	/* _FNSP_NISPLUS_ROOT_HH */
