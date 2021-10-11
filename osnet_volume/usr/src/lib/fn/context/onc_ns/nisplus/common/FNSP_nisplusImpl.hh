/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_FNSP_NISPLUSIMPL_HH
#define	_FNSP_NISPLUSIMPL_HH

#pragma ident	"@(#)FNSP_nisplusImpl.hh	1.5	97/10/27 SMI"

#include <FNSP_Impl.hh>
#include "FNSP_nisplus_address.hh"

class FNSP_nisplusImpl : public FNSP_Impl {
public:
	FNSP_nisplusImpl(FNSP_Address *);
	virtual ~FNSP_nisplusImpl();

	FN_string *get_nns_objname(const FN_string *);
	FN_ref *get_nns_ref();
	int is_this_address_type_p(const FN_ref_addr &addr);

	unsigned add_binding(const FN_string &atomic_name,
	    const FN_ref &ref, const FN_attrset *, unsigned flags);

	unsigned remove_binding(const FN_string &atomic_name);

	unsigned rename_binding(
	    const FN_string &atomic_name,
	    const FN_string &new_name,
	    unsigned flags);

	FN_ref * lookup_binding(const FN_string &atomic_name,
	    unsigned &status);

	unsigned context_exists();

	FN_ref * create_context(unsigned &status,
	    const FN_string *dirname,
	    const FN_identifier *reftype);

	unsigned destroy_context(const FN_string *dirname);

	FN_ref * create_and_bind(
	    const FN_string &child_name,
	    unsigned context_type,
	    unsigned repr_type,
	    unsigned &status,
	    int find_legal_name = FNSP_DONT_CHECK_NAME,
	    const FN_identifier *ref_type = 0,
	    const FN_attrset *attrs = 0);

	unsigned destroy_and_unbind(const FN_string &child_name);

	FN_namelist* list_names(unsigned &status,
	    int children_only);

	FN_bindinglist* list_bindings(unsigned &status);

	// Attribute operations
	FN_attrset *get_attrset(const FN_string &atomic_name,
	    unsigned &status);

	int modify_attribute(
	    const FN_string &atomic_name,
	    const FN_attribute &attr, unsigned flags);

	int set_attrset(const FN_string &stomic_name,
	    const FN_attrset &attrset);

	FN_searchlist * search_attrset(
	    const FN_attrset *attrset, unsigned int return_ref,
	    const FN_attrset *return_attr_id, unsigned int &status);
};

#endif /* _FNSP_NISPLUSIMPL_HH */
