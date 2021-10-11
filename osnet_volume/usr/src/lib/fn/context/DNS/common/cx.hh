/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */


#ifndef _FNS_DNS_CX_HH
#define	_FNS_DNS_CX_HH

#pragma ident	"@(#)cx.hh	1.7	96/03/31 SMI"

/*
 * cx.hh is a private file for the DNS_ctx module.
 * nothing should be exported from this module.
 */

#include <xfn/fn_spi.hh>

#include "dns_ops.hh"

/*
 * this implements a flat DNS context
 */

class DNS_ctx : public FN_ctx_csvc_strong {
	char		*self_domain;
	FN_ref		*self_reference;
	dns_client	*dns_cl;

	// define virtual funcs for FN_ctx:

	FN_ref		*get_ref(FN_status &) const;
	FN_composite_name *equivalent_name(
	    const FN_composite_name &name,
	    const FN_string &leading_name,
	    FN_status &status);

	// define virtual funcs for FN_ctx_csvc_strong

	FN_ref *c_lookup(
		const FN_string &name,
		unsigned int, FN_status_csvc &);
	FN_namelist *c_list_names(const FN_string &name, FN_status_csvc &);
	FN_bindinglist *c_list_bindings(
		const FN_string &name,
		FN_status_csvc &);
	int c_bind(
		const FN_string &name,
		const FN_ref &,
		unsigned bind_flags,
		FN_status_csvc &);
	int c_unbind(const FN_string &name, FN_status_csvc &);
	FN_ref *c_create_subcontext(
		const FN_string &name,
		FN_status_csvc &);
	int c_destroy_subcontext(
		const FN_string &name,
		FN_status_csvc &);

	int c_attr_multi_modify(
		const FN_string &,
		const FN_attrmodlist &,
		unsigned int,
		FN_attrmodlist **,
		FN_status_csvc &);
	FN_multigetlist *c_attr_multi_get(
		const FN_string &,
		const FN_attrset *,
		unsigned int,
		FN_status_csvc &);
	FN_attrset *c_attr_get_ids(
		const FN_string &,
		unsigned int,
		FN_status_csvc &);
	FN_valuelist *c_attr_get_values(
		const FN_string &,
		const FN_identifier &,
		unsigned int,
		FN_status_csvc &);
	int c_attr_modify(
		const FN_string &,
		unsigned int,
		const FN_attribute &,
		unsigned int,
		FN_status_csvc &);
	FN_attribute *c_attr_get(
		const FN_string &,
		const FN_identifier &,
		unsigned int,
		FN_status_csvc &);

	// Extended attribute operations
	int c_attr_bind(const FN_string &name,
	    const FN_ref &ref,
	    const FN_attrset *attrs,
	    unsigned int exclusive,
	    FN_status_csvc &status);
	FN_ref *c_attr_create_subcontext(const FN_string &name,
	    const FN_attrset *attr,
	    FN_status_csvc &status);
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

	FN_ref *c_lookup_nns(
		const FN_string &name,
		unsigned int f,
		FN_status_csvc &);
	FN_namelist *c_list_names_nns(
		const FN_string &name,
		FN_status_csvc &);
	FN_bindinglist *c_list_bindings_nns(
		const FN_string &name,
		FN_status_csvc &);
	int c_bind_nns(
		const FN_string &name,
		const FN_ref &,
		unsigned bind_flags,
		FN_status_csvc &);
	int c_unbind_nns(const FN_string &name, FN_status_csvc &);
	FN_ref *c_create_subcontext_nns(
		const FN_string &name,
		FN_status_csvc &);
	int c_destroy_subcontext_nns(
		const FN_string &name,
		FN_status_csvc &);
	int c_rename(
		const FN_string &oldname,
		const FN_composite_name &newname,
		unsigned int exclusive,
		FN_status_csvc &);
	FN_attrset *c_get_syntax_attrs(
		const FN_string &name,
		FN_status_csvc &);

	int c_attr_multi_modify_nns(
		const FN_string &,
		const FN_attrmodlist &,
		unsigned int,
		FN_attrmodlist **,
		FN_status_csvc &);
	FN_multigetlist *c_attr_multi_get_nns(
		const FN_string &,
		const FN_attrset *,
		unsigned int,
		FN_status_csvc &);
	FN_attrset *c_attr_get_ids_nns(
		const FN_string &,
		unsigned int,
		FN_status_csvc &);
	FN_valuelist *c_attr_get_values_nns(
		const FN_string &,
		const FN_identifier &,
		unsigned int,
		FN_status_csvc &);
	int c_attr_modify_nns(
		const FN_string &,
		unsigned int,
		const FN_attribute &,
		unsigned int,
		FN_status_csvc &);
	FN_attribute *c_attr_get_nns(
		const FN_string &,
		const FN_identifier &,
		unsigned int,
		FN_status_csvc &);
	FN_attrset *c_get_syntax_attrs_nns(
		const FN_string &,
		FN_status_csvc &);
	int c_rename_nns(
		const FN_string &oldname,
		const FN_composite_name &newname,
		unsigned int exclusive,
		FN_status_csvc &);

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
	// really private stuff:

	static int	trace;
public:
	DNS_ctx(const FN_ref_addr &, unsigned int authoritative);
	~DNS_ctx();
	DNS_ctx(const DNS_ctx &);		// disable default
	operator=(const DNS_ctx &);		// disable default

	static int get_trace_level() { return trace; };
	static int trace_level(int level);
	// free agents
	static int addrs_from_txt(int ac, const char **av, FN_ref &ref,
	    FN_status &s);
	static int get_names(dns_client *dns_cl, const char *dom,
	    dns_rr_vec &names, int all, unsigned int authoritative);
};

#endif /* _FNS_DNS_CX_HH */
