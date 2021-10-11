/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_X500CONTEXT_HH
#define	_X500CONTEXT_HH

#pragma ident	"@(#)X500Context.hh	1.1	96/03/31 SMI"


#include "X500DUA.hh"
#include "X500Trace.hh"


/*
 * The X.500 context
 */


enum access_protocol {
	LDAP_ACCESS,
	DAP_ACCESS,
	NONE
};


class X500Context : public FN_ctx_csvc_weak_static, public X500Trace
{

	static access_protocol	x500_access;	// DAP or LDAP

	FN_string		*context_prefix;
	FN_ref			*self_reference;
	X500DUA			*x500_dua;

	FN_string		*xfn_name_to_x500(const FN_string &name,
				    FN_status_csvc &cs);

	// to determine NS boundary
	FN_composite_name	*p_component_parser(const FN_composite_name &,
				    FN_composite_name **rest,
				    FN_status_psvc &s);

	// define virtual funcs for FN_ctx:
	FN_ref			*get_ref(FN_status &s) const;

	FN_composite_name	*equivalent_name(const FN_composite_name &name,
				    const FN_string &leading_name,
				    FN_status &s);

	// define virtual funcs for FN_ctx_csvc_weak_static
	FN_ref			*c_lookup(const FN_string &name,
				    unsigned int lookup_flags,
				    FN_status_csvc &);

	FN_ref			*c_lookup_nns(const FN_string &name,
				    unsigned int f, FN_status_csvc &);

	FN_namelist		*c_list_names(const FN_string &name,
				    FN_status_csvc &);

	FN_namelist		*c_list_names_nns(const FN_string &name,
				    FN_status_csvc &);

	FN_bindinglist		*c_list_bindings(const FN_string &name,
				    FN_status_csvc &);

	FN_bindinglist		*c_list_bindings_nns(const FN_string &name,
				    FN_status_csvc &);

	int			c_bind(const FN_string &name, const FN_ref &,
				    unsigned bind_flags, FN_status_csvc &);

	int			c_bind_nns(const FN_string &name,
				    const FN_ref &, unsigned bind_flags,
				    FN_status_csvc &);

	int			c_unbind(const FN_string &name,
				    FN_status_csvc &);

	int			c_unbind_nns(const FN_string &name,
				    FN_status_csvc &);

	FN_ref			*c_create_subcontext(const FN_string &name,
				    FN_status_csvc &);

	FN_ref			*c_create_subcontext_nns(const FN_string &name,
				    FN_status_csvc &);

	int			c_destroy_subcontext(const FN_string &name,
				    FN_status_csvc &);

	int			c_destroy_subcontext_nns(const FN_string &name,
				    FN_status_csvc &);

	int			c_rename(const FN_string &oldname,
				    const FN_composite_name &newname,
				    unsigned int exclusive, FN_status_csvc &);

	int			c_rename_nns(const FN_string &oldname,
				    const FN_composite_name &newname,
				    unsigned int exclusive, FN_status_csvc &);

	FN_attrset		*c_get_syntax_attrs(const FN_string &name,
				    FN_status_csvc &);

	FN_attrset		*c_get_syntax_attrs_nns(const FN_string &,
				    FN_status_csvc &);

	FN_attribute		*c_attr_get(const FN_string &,
				    const FN_identifier &, unsigned int,
				    FN_status_csvc &);

	FN_attribute		*c_attr_get_nns(const FN_string &,
				    const FN_identifier &, unsigned int,
				    FN_status_csvc &);

	FN_attrset		*c_attr_get_ids(const FN_string &, unsigned int,
				    FN_status_csvc &);

	FN_attrset		*c_attr_get_ids_nns(const FN_string &,
				    unsigned int, FN_status_csvc &);

	FN_valuelist		*c_attr_get_values(const FN_string &,
				    const FN_identifier &, unsigned int,
				    FN_status_csvc &);

	FN_valuelist		*c_attr_get_values_nns(const FN_string &,
				    const FN_identifier &, unsigned int,
				    FN_status_csvc &);

	FN_multigetlist		*c_attr_multi_get(const FN_string &,
				    const FN_attrset *, unsigned int,
				    FN_status_csvc &);

	FN_multigetlist		*c_attr_multi_get_nns(const FN_string &,
				    const FN_attrset *, unsigned int,
				    FN_status_csvc &);

	int			c_attr_modify(const FN_string &, unsigned int,
				    const FN_attribute &, unsigned int,
				    FN_status_csvc &);

	int			c_attr_modify_nns(const FN_string &,
				    unsigned int, const FN_attribute &,
				    unsigned int, FN_status_csvc &);

	int			c_attr_multi_modify_nns(const FN_string &,
				    const FN_attrmodlist &, unsigned int,
				    FN_attrmodlist **, FN_status_csvc &);

	int			c_attr_multi_modify(const FN_string &,
				    const FN_attrmodlist &, unsigned int,
				    FN_attrmodlist **, FN_status_csvc &);

	// Extended attribute operations
	FN_searchlist		*c_attr_search(const FN_string &name,
				    const FN_attrset *match_attrs,
				    unsigned int return_ref,
				    const FN_attrset *return_attr_ids,
				    FN_status_csvc &);

	FN_searchlist		*c_attr_search_nns(const FN_string &name,
				    const FN_attrset *match_attrs,
				    unsigned int return_ref,
				    const FN_attrset *return_attr_ids,
				    FN_status_csvc &);

	FN_ext_searchlist	*c_attr_ext_search(const FN_string &name,
				    const FN_search_control *control,
				    const FN_search_filter *filter,
				    FN_status_csvc &);

	FN_ext_searchlist	*c_attr_ext_search_nns(const FN_string &name,
				    const FN_search_control *control,
				    const FN_search_filter *filter,
				    FN_status_csvc &);

	int			c_attr_bind(const FN_string &name,
				    const FN_ref &ref, const FN_attrset *attrs,
				    unsigned int exclusive, FN_status_csvc &);

	int			c_attr_bind_nns(const FN_string &name,
				    const FN_ref &ref, const FN_attrset *attrs,
				    unsigned int exclusive, FN_status_csvc &);

	FN_ref			*c_attr_create_subcontext(const FN_string &name,
				    const FN_attrset *attr, FN_status_csvc &);

	FN_ref			*c_attr_create_subcontext_nns(
				    const FN_string &name,
				    const FN_attrset *attr, FN_status_csvc &);

public:
	X500Context(const FN_ref_addr &addr, const FN_ref &ref,
	    unsigned int auth, int &err);

	virtual ~X500Context();
};


#endif	/* _X500CONTEXT_HH */
