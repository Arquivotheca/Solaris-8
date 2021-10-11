/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_ENSCONTEXT_HH
#define	_FNSP_ENSCONTEXT_HH

#pragma ident	"@(#)FNSP_ENSContext.hh	1.4	96/03/31 SMI"

#include <xfn/fn_spi.hh>
#include <FNSP_Address.hh>

class FNSP_ENSContext : public FN_ctx_csvc_strong {
private:
	FN_ref *resolve(const FN_string &name, FN_status_csvc&);
	void handle_attr_operation(const FN_string &, FN_status_csvc &);
	FN_ref *my_reference;
	FN_ref *org_ref;
	FN_ref *org_nns_ref;
	FNSP_Address *org_nns_addr;

public:
#ifdef DEBUG
	FNSP_ENSContext(const FN_string &dirname, unsigned int auth = 0);
#endif

	FNSP_ENSContext(const FN_ref_addr &addr, const FN_ref &ref,
	    unsigned int auth);
	~FNSP_ENSContext();
	FN_ref *get_ref(FN_status &stat) const;
	FN_composite_name *equivalent_name(
	    const FN_composite_name &name,
	    const FN_string &leading_name,
	    FN_status &status);

	static FNSP_ENSContext* from_address(const FN_ref_addr &addr,
	    const FN_ref &ref, unsigned int auth,
	    FN_status &stat);

	FN_ref *c_lookup(const FN_string &name,
	    unsigned int f, FN_status_csvc&);
	FN_namelist* c_list_names(const FN_string &name, FN_status_csvc&);
	FN_bindinglist* c_list_bindings(const FN_string &name,
	    FN_status_csvc&);
	int c_bind(const FN_string &name, const FN_ref &,
	    unsigned bind_flags, FN_status_csvc&);
	int c_unbind(const FN_string &name, FN_status_csvc&);
	FN_ref *c_create_subcontext(const FN_string &name,
	    FN_status_csvc&);
	int c_destroy_subcontext(const FN_string &name, FN_status_csvc&);
	int c_rename(const FN_string &oldname,
	    const FN_composite_name &newname,
	    unsigned int exclusive,
	    FN_status_csvc&);
	FN_attrset* c_get_syntax_attrs(const FN_string &name,
	    FN_status_csvc&);
	// Attribute operations
	FN_attribute *c_attr_get(const FN_string &,
	    const FN_identifier &, unsigned int,
	    FN_status_csvc&);
	int c_attr_modify(const FN_string &,
	    unsigned int,
	    const FN_attribute&, unsigned int,
	    FN_status_csvc&);
	FN_valuelist *c_attr_get_values(const FN_string &,
	    const FN_identifier &, unsigned int,
	    FN_status_csvc&);
	FN_attrset *c_attr_get_ids(const FN_string &, unsigned int,
	    FN_status_csvc&);
	FN_multigetlist *c_attr_multi_get(const FN_string &,
	    const FN_attrset *, unsigned int,
	    FN_status_csvc&);
	int c_attr_multi_modify(const FN_string &,
	    const FN_attrmodlist&, unsigned int,
	    FN_attrmodlist **,
	    FN_status_csvc&);

	// Extended Attribute interface
	int c_attr_bind(const FN_string &name,
			    const FN_ref &ref,
			    const FN_attrset *attrs,
			    unsigned int exclusive,
			    FN_status_csvc &status);
	FN_ref *c_attr_create_subcontext(const FN_string &name,
	    const FN_attrset *attr, FN_status_csvc &status);
	FN_searchlist *c_attr_search(
	    const FN_string &name,
	    const FN_attrset *match_attrs,
	    unsigned int return_ref,
	    const FN_attrset *return_attr_ids,
	    FN_status_csvc &status);
	FN_ext_searchlist *c_attr_ext_search(
	    const FN_string &name,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status_csvc &status);

	FN_ref *c_lookup_nns(const FN_string &name, unsigned int f,
	    FN_status_csvc&);
	FN_namelist* c_list_names_nns(const FN_string &name,
	    FN_status_csvc&);
	FN_bindinglist* c_list_bindings_nns(const FN_string &name,
	    FN_status_csvc&);
	int c_bind_nns(const FN_string &name, const FN_ref &,
	    unsigned bind_flags, FN_status_csvc&);
	int c_unbind_nns(const FN_string &name, FN_status_csvc&);
	FN_ref *c_create_subcontext_nns(const FN_string &name,
	    FN_status_csvc&);
	int c_destroy_subcontext_nns(const FN_string &name,
	    FN_status_csvc&);
	int c_rename_nns(const FN_string &oldname,
	    const FN_composite_name &newname,
	    unsigned int exclusive,
	    FN_status_csvc&);
	FN_attrset* c_get_syntax_attrs_nns(const FN_string &name,
	    FN_status_csvc&);
	// Attribute operations
	FN_attribute *c_attr_get_nns(const FN_string &,
	    const FN_identifier &, unsigned int,
	    FN_status_csvc&);
	int c_attr_modify_nns(const FN_string &,
	    unsigned int,
	    const FN_attribute&, unsigned int,
	    FN_status_csvc&);
	FN_valuelist *c_attr_get_values_nns(const FN_string &,
	    const FN_identifier &, unsigned int,
	    FN_status_csvc&);
	FN_attrset *c_attr_get_ids_nns(const FN_string &,
	    unsigned int, FN_status_csvc&);
	FN_multigetlist *c_attr_multi_get_nns(const FN_string &,
	    const FN_attrset *, unsigned int,
	    FN_status_csvc&);
	int c_attr_multi_modify_nns(const FN_string &,
	    const FN_attrmodlist&, unsigned int,
	    FN_attrmodlist **,
	    FN_status_csvc&);

	// Extended attribute operations on Next naming system pointer
	int c_attr_bind_nns(const FN_string &name,
			    const FN_ref &ref,
			    const FN_attrset *attrs,
			    unsigned int exclusive,
			    FN_status_csvc &status);
	FN_ref *c_attr_create_subcontext_nns(const FN_string &name,
	    const FN_attrset *attr, FN_status_csvc &status);
	FN_searchlist *c_attr_search_nns(
	    const FN_string &name,
	    const FN_attrset *match_attrs,
	    unsigned int return_ref,
	    const FN_attrset *return_attr_ids,
	    FN_status_csvc &status);
	FN_ext_searchlist *c_attr_ext_search_nns(
	    const FN_string &name,
	    const FN_search_control *control,
	    const FN_search_filter *filter,
	    FN_status_csvc &status);
};

#endif	/* _FNSP_ENSCONTEXT_HH */
