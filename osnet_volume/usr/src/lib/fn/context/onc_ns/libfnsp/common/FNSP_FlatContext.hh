/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_FNSP_FLATCONTEXT_HH
#define	_FNSP_FLATCONTEXT_HH

#pragma ident	"@(#)FNSP_FlatContext.hh	1.1	96/03/31 SMI"

#include <xfn/fn_spi.hh>
#include "FNSP_Impl.hh"

/*
 * For supporting flat naming systems; these include:
 * - Namespace ID context
 * - Username context
 * - Hostname context
 * Username and hostname contexts use this as their base class.
 */

class FNSP_FlatContext : public FN_ctx_csvc_strong {
protected:
	FNSP_Impl *ns_impl;
	FN_ref *my_reference;  /* encoded */

	// internal functions
	virtual FN_ref *resolve(const FN_string &,
	    unsigned int flags, FN_status_csvc&);

	virtual int
	    is_following_link(const FN_string &name, FN_status_csvc &cstat);
public:
	virtual ~FNSP_FlatContext();

	virtual FN_ref *get_ref(FN_status &)const;
	virtual FN_composite_name *equivalent_name(
	    const FN_composite_name &name,
	    const FN_string &leading_name,
	    FN_status &status);

	// implementations for Compound Interface
	virtual FN_ref *c_lookup(const FN_string &name, unsigned int f,
	    FN_status_csvc&);
	virtual FN_namelist* c_list_names(const FN_string &n, FN_status_csvc&);
	virtual FN_bindinglist* c_list_bindings(const FN_string &name,
	    FN_status_csvc&);
	virtual int c_bind(const FN_string &name, const FN_ref &,
	    unsigned BindFlags, FN_status_csvc&);
	virtual int c_unbind(const FN_string &name, FN_status_csvc&);
	virtual int c_rename(const FN_string &name, const FN_composite_name &,
	    unsigned BindFlags, FN_status_csvc&);
	virtual FN_ref *c_create_subcontext(const FN_string &name,
	    FN_status_csvc&);
	virtual int c_destroy_subcontext(const FN_string &name,
	    FN_status_csvc&);
	virtual FN_attrset* c_get_syntax_attrs(const FN_string &name,
	    FN_status_csvc&);

	// Attribute Operations
	virtual FN_attribute *c_attr_get(const FN_string &,
	    const FN_identifier &, unsigned int,
	    FN_status_csvc&);
	virtual int c_attr_modify(const FN_string &,
	    unsigned int,
	    const FN_attribute&, unsigned int,
	    FN_status_csvc&);
	virtual FN_valuelist *c_attr_get_values(const FN_string &,
	    const FN_identifier &, unsigned int,
	    FN_status_csvc&);
	virtual FN_attrset *c_attr_get_ids(const FN_string &,
	    unsigned int, FN_status_csvc&);
	virtual FN_multigetlist *c_attr_multi_get(const FN_string &,
	    const FN_attrset *, unsigned int,
	    FN_status_csvc&);
	virtual int c_attr_multi_modify(const FN_string &,
	    const FN_attrmodlist&, unsigned int,
	    FN_attrmodlist **,
	    FN_status_csvc&);

	virtual int c_attr_bind(const FN_string &name,
	    const FN_ref &ref,
	    const FN_attrset *attrs,
	    unsigned int exclusive,
	    FN_status_csvc &status);
	virtual FN_ref *c_attr_create_subcontext(const FN_string &name,
	    const FN_attrset *attr, FN_status_csvc &status);
	virtual FN_searchlist *c_attr_search(
	    const FN_string &name,
	    const FN_attrset *match_attrs,
	    unsigned int return_ref,
	    const FN_attrset *return_attr_ids,
	    FN_status_csvc &status);
	virtual FN_ext_searchlist *c_attr_ext_search(
	    const FN_string &name,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status_csvc &status);

	virtual FN_ref *c_lookup_nns(const FN_string &name, unsigned int f,
	    FN_status_csvc&);
	virtual FN_namelist* c_list_names_nns(const FN_string &name,
					    FN_status_csvc&);
	virtual FN_bindinglist* c_list_bindings_nns(const FN_string &name,
	    FN_status_csvc&);
	virtual int c_bind_nns(const FN_string &name, const FN_ref &,
	    unsigned BindFlags, FN_status_csvc&);
	virtual int c_unbind_nns(const FN_string &name, FN_status_csvc&);
	virtual int c_rename_nns(const FN_string &name,
	    const FN_composite_name &, unsigned BindFlags, FN_status_csvc&);
	virtual FN_ref *c_create_subcontext_nns(const FN_string &name,
	    FN_status_csvc&);
	virtual int c_destroy_subcontext_nns(const FN_string &name,
					    FN_status_csvc&);
	virtual FN_attrset* c_get_syntax_attrs_nns(const FN_string &name,
	    FN_status_csvc&);

	// Attribute Operations
	virtual FN_attribute *c_attr_get_nns(const FN_string &,
	    const FN_identifier &, unsigned int,
	    FN_status_csvc&);
	virtual int c_attr_modify_nns(const FN_string &,
	    unsigned int,
	    const FN_attribute&, unsigned int,
	    FN_status_csvc&);
	virtual FN_valuelist *c_attr_get_values_nns(const FN_string &,
	    const FN_identifier &, unsigned int,
	    FN_status_csvc&);
	virtual FN_attrset *c_attr_get_ids_nns(const FN_string &,
	    unsigned int, FN_status_csvc&);
	virtual FN_multigetlist *c_attr_multi_get_nns(const FN_string &,
	    const FN_attrset *, unsigned int,
	    FN_status_csvc&);
	virtual int c_attr_multi_modify_nns(const FN_string &,
	    const FN_attrmodlist&, unsigned int,
	    FN_attrmodlist **,
	    FN_status_csvc&);

	virtual int c_attr_bind_nns(const FN_string &name,
	    const FN_ref &ref,
	    const FN_attrset *attrs,
	    unsigned int exclusive,
	    FN_status_csvc &status);
	virtual FN_ref *c_attr_create_subcontext_nns(const FN_string &name,
	    const FN_attrset *attr, FN_status_csvc &status);
	virtual FN_searchlist *c_attr_search_nns(
	    const FN_string &name,
	    const FN_attrset *match_attrs,
	    unsigned int return_ref,
	    const FN_attrset *return_attr_ids,
	    FN_status_csvc &status);
	virtual FN_ext_searchlist *c_attr_ext_search_nns(
	    const FN_string &name,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status_csvc &status);
};

#endif	/* _FNSP_FLATCONTEXT_HH */
