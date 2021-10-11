/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */


#ifndef	_FNSP_FILESPRINTERNAMECONTEXT_HH
#define	_FNSP_FILESPRINTERNAMECONTEXT_HH

#pragma ident	"@(#)FNSP_filesPrinternameContext.hh	1.1	96/03/31 SMI"

#include <FNSP_PrinternameContext.hh>

class FNSP_filesPrinternameContext : public FNSP_PrinternameContext {
protected:
	int fns_installed() { return (ns_impl != 0); };

	FN_ref* resolve(const FN_string &, FN_status_csvc &);
public:
	FNSP_filesPrinternameContext(const FN_ref_addr &, const FN_ref &);
	~FNSP_filesPrinternameContext();

	static FNSP_filesPrinternameContext* from_address(const FN_ref_addr&,
	    const FN_ref&, FN_status& stat);

	int c_bind(const FN_string &name, const FN_ref&,
	    unsigned BindFlags, FN_status_csvc&);
	int c_unbind(const FN_string &name, FN_status_csvc&);
	int c_rename(const FN_string &name, const FN_composite_name &,
	    unsigned BindFlags, FN_status_csvc&);
	FN_ref* c_create_subcontext(const FN_string &name, FN_status_csvc&);
	int c_destroy_subcontext(const FN_string &name, FN_status_csvc&);
	// Attribute Operations
	FN_attribute *c_attr_get(const FN_string&,
	    const FN_identifier&, unsigned int, FN_status_csvc&);
	int c_attr_modify(const FN_string&,
	    unsigned int, const FN_attribute&, unsigned int, FN_status_csvc&);
	FN_valuelist *c_attr_get_values(const FN_string&,
	    const FN_identifier&, unsigned int, FN_status_csvc&);
	FN_attrset *c_attr_get_ids(const FN_string&, unsigned int,
	    FN_status_csvc&);
	FN_multigetlist *c_attr_multi_get(const FN_string&,
	    const FN_attrset *, unsigned int, FN_status_csvc&);
	int c_attr_multi_modify(const FN_string&,
	    const FN_attrmodlist&, unsigned int,
	    FN_attrmodlist **, FN_status_csvc&);

	// Externed attribute operations
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
};

#endif	/* _FNSP_FILESPRINTERNAMECONTEXT_HH */
