/*
 * Copyright (c) 1992 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_ATTRS_HH
#define	_FNSP_ATTRS_HH

#pragma ident	"@(#)fnsp_attrs.hh	1.3	96/09/04 SMI"

#include <xfn/xfn.hh>
#include <rpcsvc/nis.h>

#include <FNSP_Address.hh>

// funtions to support attribute opertations
// To obtains the attribute set
extern FN_attrset *
FNSP_get_attrset(const FNSP_Address &context, const FN_string &atomic_name,
    unsigned &status);

// Attribute support for single table implementation
// That is for username and hostname implementation
extern FN_attribute*
FNSP_get_attribute(const FNSP_Address &context, const FN_string &aname,
    const FN_identifier &id, unsigned &status);

extern int
FNSP_set_attribute(const FNSP_Address &context, const FN_string &aname,
    const FN_attribute &attr);

extern int
FNSP_remove_attribute(const FNSP_Address &context, const FN_string &aname,
    const FN_attribute *attribute = 0);

extern int
FNSP_remove_attribute_values(const FNSP_Address &context,
    const FN_string &aname,
    const FN_attribute &attribute);

// used by fnsp_*
extern FN_attrset*
FNSP_extract_attrset_result(nis_result *res, unsigned &status);

extern int
FNSP_store_attrset(const FN_attrset *aset, char **retbuf, size_t *retsize);

extern int
FNSP_create_attribute_table_base(const FN_string &tabname,
    unsigned int access_flags,
    unsigned string_case);

#endif /* _FNSP_ATTRS_HH */
