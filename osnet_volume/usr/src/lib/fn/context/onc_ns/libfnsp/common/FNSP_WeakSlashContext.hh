/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_WEAKSLASHCONTEXT_HH
#define	_FNSP_WEAKSLASHCONTEXT_HH

#pragma ident	"@(#)FNSP_WeakSlashContext.hh	1.1	96/03/31 SMI"

#include <xfn/fn_spi.hh>
#include "FNSP_Impl.hh"

/* For:  service context */
class FNSP_WeakSlashContext : public FN_ctx_asvc_weak_dynamic {
protected:
	FN_ref *my_reference;  /* encoded */
	FNSP_Impl *ns_impl;

	virtual FN_ref *resolve(const FN_string &name,
	    unsigned int lookup_flags, FN_status_asvc&);

	virtual is_following_link(const FN_string &name,
	    FN_status_asvc &as);

public:
	virtual ~FNSP_WeakSlashContext();
	virtual FN_ref *get_ref(FN_status &)const;
	virtual FN_composite_name *equivalent_name(
	    const FN_composite_name &name,
	    const FN_string &leading_name,
	    FN_status &status);

	// atomic name service interface
	virtual FN_ref *a_lookup(const FN_string &name, unsigned int f,
	    FN_status_asvc&);
	virtual FN_namelist* a_list_names(FN_status_asvc&);
	virtual FN_bindinglist* a_list_bindings(FN_status_asvc&);
	virtual int a_rename(const FN_string &name, const FN_composite_name &,
	    unsigned BindFlags, FN_status_asvc&);
	virtual int a_bind(const FN_string &name, const FN_ref &,
	    unsigned BindFlags, FN_status_asvc&);
	virtual int a_unbind(const FN_string &name, FN_status_asvc&);
	virtual FN_ref *a_create_subcontext(const FN_string &name,
	    FN_status_asvc&);
	virtual int a_destroy_subcontext(const FN_string &name,
	    FN_status_asvc&);
	virtual FN_attrset* a_get_syntax_attrs(FN_status_asvc&);
	// Attribute operations
	virtual FN_attribute *a_attr_get(const FN_string &,
	    const FN_identifier &, unsigned int,
	    FN_status_asvc&);
	virtual int a_attr_modify(const FN_string &,
	    unsigned int,
	    const FN_attribute&, unsigned int,
	    FN_status_asvc&);
	virtual FN_valuelist *a_attr_get_values(const FN_string &,
	    const FN_identifier &, unsigned int,
	    FN_status_asvc&);
	virtual FN_attrset *a_attr_get_ids(const FN_string &,
	    unsigned int, FN_status_asvc&);
	virtual FN_multigetlist *a_attr_multi_get(const FN_string &,
	    const FN_attrset *, unsigned int,
	    FN_status_asvc&);
	virtual int a_attr_multi_modify(const FN_string &,
	    const FN_attrmodlist&, unsigned int, FN_attrmodlist **,
	    FN_status_asvc&);
	// Extended attribute operations
	virtual int a_attr_bind(const FN_string &name,
	    const FN_ref &ref,
	    const FN_attrset *attrs,
	    unsigned int exclusive,
	    FN_status_asvc &status);
	virtual FN_ref *a_attr_create_subcontext(const FN_string &name,
	    const FN_attrset *attr, FN_status_asvc &status);
	virtual FN_searchlist *a_attr_search(
	    const FN_attrset *match_attrs,
	    unsigned int return_ref,
	    const FN_attrset *return_attr_ids,
	    FN_status_asvc &status);
	virtual FN_ext_searchlist *a_attr_ext_search(
	    const FN_string &name,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status_asvc &status);

	virtual FN_ref *a_lookup_nns(const FN_string&, unsigned int f,
	    FN_status_asvc&);
	virtual int a_bind_nns(const FN_string&,
	    const FN_ref &, unsigned BindFlags, FN_status_asvc&);
	virtual int a_unbind_nns(const FN_string&, FN_status_asvc&);
	virtual int a_rename_nns(const FN_string&,
	    const FN_composite_name &newn, unsigned f, FN_status_asvc&);
	virtual FN_ref *a_create_subcontext_nns(const FN_string&,
	    FN_status_asvc&);
	virtual int a_destroy_subcontext_nns(const FN_string&, FN_status_asvc&);

	// Attribute operations
	virtual FN_attribute *a_attr_get_nns(const FN_string &,
	    const FN_identifier &, unsigned int,
	    FN_status_asvc&);
	virtual int a_attr_modify_nns(const FN_string &,
	    unsigned int,
	    const FN_attribute&, unsigned int,
	    FN_status_asvc&);
	virtual FN_valuelist *a_attr_get_values_nns(const FN_string &,
	    const FN_identifier &, unsigned int,
	    FN_status_asvc&);
	virtual FN_attrset *a_attr_get_ids_nns(const FN_string &, unsigned int,
	    FN_status_asvc&);
	virtual FN_multigetlist *a_attr_multi_get_nns(const FN_string &,
	    const FN_attrset *, unsigned int,
	    FN_status_asvc&);
	virtual int a_attr_multi_modify_nns(const FN_string &,
	    const FN_attrmodlist&, unsigned int,
	    FN_attrmodlist **,
	    FN_status_asvc&);

	// Extended attribute operations
	virtual int a_attr_bind_nns(const FN_string &name,
	    const FN_ref &ref,
	    const FN_attrset *attrs,
	    unsigned int exclusive,
	    FN_status_asvc &status);
	virtual FN_ref *a_attr_create_subcontext_nns(const FN_string &name,
	    const FN_attrset *attr, FN_status_asvc &status);
	virtual FN_ext_searchlist *a_attr_ext_search_nns(
	    const FN_string &name,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status_asvc &status);
};

#endif	/* _FNSP_WEAKSLASHCONTEXT_HH */
