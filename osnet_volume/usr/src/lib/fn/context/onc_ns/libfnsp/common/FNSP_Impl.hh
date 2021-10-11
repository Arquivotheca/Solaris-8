/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_FNSP_IMPL_HH
#define	_FNSP_IMPL_HH

#pragma ident	"@(#)FNSP_Impl.hh	1.6	97/10/23 SMI"

#include <xfn/xfn.hh>
#include <xfn/FN_nameset.hh>
#include <xfn/FN_bindingset.hh>
#include <xfn/FN_searchset.hh>

#include "FNSP_Address.hh"

enum FNSP_legal_table_check {
	FNSP_DONT_CHECK_NAME = 0,
	FNSP_CHECK_NAME = 1
	};

class FNSP_Impl {
public:
	FNSP_Impl(FNSP_Address *);
	virtual ~FNSP_Impl();
	FNSP_Address *my_address;

	virtual FN_string *get_nns_objname(const FN_string *) = 0;
	virtual FN_ref *get_nns_ref() = 0;
	virtual int is_this_address_type_p(const FN_ref_addr &) = 0;

	int check_if_old_addr_present(const FN_ref &oref,
	    const FN_ref &newref);

	virtual unsigned add_binding(const FN_string &atomic_name,
	    const FN_ref &ref, const FN_attrset *, unsigned flags) = 0;

	virtual unsigned remove_binding(const FN_string &atomic_name) = 0;

	virtual unsigned rename_binding(
	    const FN_string &atomic_name,
	    const FN_string &new_name,
	    unsigned flags) = 0;

	virtual FN_ref * lookup_binding(const FN_string &atomic_name,
	    unsigned &status) = 0;

	virtual unsigned context_exists() = 0;

	virtual FN_ref * create_context(unsigned &status,
	    const FN_string *dirname = 0,
	    const FN_identifier *reftype = 0) = 0;

	virtual unsigned destroy_context(const FN_string *dirname = 0) = 0;

	virtual FN_ref * create_and_bind(
	    const FN_string &child_name,
	    unsigned context_type,
	    unsigned repr_type,
	    unsigned &status,
	    int find_legal_name = 0,
	    const FN_identifier *ref_type = 0,
	    const FN_attrset *attrs = 0) = 0;

	virtual unsigned destroy_and_unbind(const FN_string &child_name) = 0;

	virtual FN_namelist* list_names(unsigned &stat, int chld_only = 0) = 0;

	virtual FN_bindinglist* list_bindings(unsigned &status) = 0;

	// Attribute operations
	virtual FN_attrset *get_attrset(const FN_string &atomic_name,
	    unsigned &status) = 0;

	virtual int modify_attribute(
	    const FN_string &atomic_name,
	    const FN_attribute &attr, unsigned flags) = 0;

	virtual int set_attrset(const FN_string &stomic_name,
	    const FN_attrset &attrset) = 0;

	virtual FN_searchlist * search_attrset(
	    const FN_attrset *attrset, unsigned int return_ref,
	    const FN_attrset *return_attr_id, unsigned int &status) = 0;
};

#endif /* _FNSP_IMPL_HH */
