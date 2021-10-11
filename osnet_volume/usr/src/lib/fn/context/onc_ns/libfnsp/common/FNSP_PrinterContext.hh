/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_FNSP_PRINTERCONTEXT_HH
#define	_FNSP_PRINTERCONTEXT_HH

#pragma ident	"@(#)FNSP_PrinterContext.hh	1.1	96/03/31 SMI"

#include <xfn/fn_spi.hh>

extern FN_ref*
get_service_ref_from_value(const FN_string& intname, char *value);

class FNSP_PrinterContext : public FN_ctx_csvc_strong {
protected:
	FN_ref *my_reference;

	// internal construction function
	FNSP_PrinterContext(const FN_ref_addr&, const FN_ref&,
	    unsigned int auth);

	virtual FN_ref* resolve(const FN_string&, FN_status_csvc&);
	virtual FN_nameset* list(FN_status_csvc&);
	virtual FN_bindingset* list_bs(FN_status_csvc&);

public:
	~FNSP_PrinterContext();

	static FNSP_PrinterContext* from_address(const FN_ref_addr &,
	    const FN_ref &, unsigned int auth, FN_status &stat);

	virtual FN_ref* get_ref(FN_status &) const;
	virtual FN_composite_name *equivalent_name(
	    const FN_composite_name &name,
	    const FN_string &leading_name,
	    FN_status &status);

	// implementations for FNS Printername Context
	virtual FN_ref* c_lookup(const FN_string &name, unsigned int f,
	    FN_status_csvc&);
	virtual FN_namelist*
	    c_list_names(const FN_string &name, FN_status_csvc&);
	virtual FN_bindinglist*
	    c_list_bindings(const FN_string &name, FN_status_csvc&);
	virtual int c_bind(const FN_string &name, const FN_ref&,
	    unsigned BindFlags, FN_status_csvc&);
	virtual int c_unbind(const FN_string &name, FN_status_csvc&);
	virtual int c_rename(const FN_string &name,
	    const FN_composite_name &,
	    unsigned BindFlags, FN_status_csvc &);
	virtual FN_ref* c_create_subcontext(const FN_string &name,
	    FN_status_csvc &);
	virtual int c_destroy_subcontext(const FN_string &name,
	    FN_status_csvc &);
	virtual FN_attrset*
	    c_get_syntax_attrs(const FN_string &name, FN_status_csvc &);

	// Attribute Operations
	virtual FN_attribute *c_attr_get(const FN_string&,
	    const FN_identifier&, unsigned int, FN_status_csvc&);
	virtual int c_attr_modify(const FN_string&, unsigned int,
	    const FN_attribute&, unsigned int, FN_status_csvc&);
	virtual FN_valuelist *c_attr_get_values(const FN_string&,
	    const FN_identifier&, unsigned int, FN_status_csvc&);
	virtual FN_attrset *c_attr_get_ids(const FN_string&,
	    unsigned int, FN_status_csvc&);
	virtual FN_multigetlist *c_attr_multi_get(const FN_string&,
	    const FN_attrset *, unsigned int, FN_status_csvc&);
	virtual int c_attr_multi_modify(const FN_string&,
	    const FN_attrmodlist&, unsigned int,
	    FN_attrmodlist **, FN_status_csvc&);

	// Externed attribute operations
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

	// Operations on next naming system pointer
	virtual FN_ref* c_lookup_nns(const FN_string& name,
	    unsigned int f, FN_status_csvc&);
	virtual FN_namelist* c_list_names_nns(const FN_string& name,
	    FN_status_csvc&);
	virtual FN_bindinglist* c_list_bindings_nns(
	    const FN_string &name,
	    FN_status_csvc&);
	virtual int c_bind_nns(const FN_string& name, const FN_ref&,
	    unsigned BindFlags, FN_status_csvc&);
	virtual int c_unbind_nns(const FN_string& name, FN_status_csvc&);
	virtual int c_rename_nns(const FN_string& name,
	    const FN_composite_name&,
	    unsigned BindFlags, FN_status_csvc&);
	virtual FN_ref* c_create_subcontext_nns(const FN_string& name,
	    FN_status_csvc&);
	virtual int c_destroy_subcontext_nns(const FN_string& name,
	    FN_status_csvc&);
	virtual FN_attrset* c_get_syntax_attrs_nns(
	    const FN_string& name,
	    FN_status_csvc&);

	// Attribute Operations
	virtual FN_attribute *c_attr_get_nns(const FN_string&,
	    const FN_identifier&, unsigned int, FN_status_csvc&);
	virtual int c_attr_modify_nns(const FN_string&,
	    unsigned int, const FN_attribute&, unsigned int, FN_status_csvc&);
	virtual FN_valuelist *c_attr_get_values_nns(const FN_string&,
	    const FN_identifier&, unsigned int, FN_status_csvc&);
	virtual FN_attrset *c_attr_get_ids_nns(const FN_string&,
	    unsigned int,
	    FN_status_csvc&);
	virtual FN_multigetlist *c_attr_multi_get_nns(const FN_string&,
	    const FN_attrset *, unsigned int, FN_status_csvc&);
	virtual int c_attr_multi_modify_nns(const FN_string&,
	    const FN_attrmodlist&, unsigned int, FN_attrmodlist **,
	    FN_status_csvc&);

	// Externed attribute operations on nns
	virtual int c_attr_bind_nns(const FN_string &name,
			    const FN_ref &ref,
			    const FN_attrset *attrs,
			    unsigned int exclusive,
			    FN_status_csvc &status);
	virtual FN_ref *c_attr_create_subcontext_nns(
	    const FN_string &name,
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

#endif	/* _FNSP_PRINTERCONTEXT_HH */
