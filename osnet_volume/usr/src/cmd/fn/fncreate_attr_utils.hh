/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNCREATE_ATTR_UTILS_HH
#define	_FNCREATE_ATTR_UTILS_HH

#pragma ident	"@(#)fncreate_attr_utils.hh	1.1	96/03/31 SMI"

#include <xfn/xfn.hh>

extern FN_attrset *
generate_creation_attrs(unsigned int context_type,
    const FN_identifier *ref_type);

#endif	/* _FNCREATE_ATTR_UTILS_HH */
