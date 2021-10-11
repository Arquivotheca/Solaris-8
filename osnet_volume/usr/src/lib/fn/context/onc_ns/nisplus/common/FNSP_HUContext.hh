/*
 * Copyright (c) 1992 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_HUCONTEXT_HH
#define	_FNSP_HUCONTEXT_HH

#pragma ident	"@(#)FNSP_HUContext.hh	1.9	96/10/11 SMI"

#include <xfn/fn_spi.hh>
#include <xfn/fn_p.hh>
#include <xfn/FN_searchset.hh>

#include <FNSP_Address.hh>
#include "FNSP_nisplusFlatContext.hh"

// A HU naming system is one in which host or user names are bound.
// All names in the naming system are bound in a single context.
// The only thing that can be bound under a name is a nns pointer,
// and are implemented as junctions.
//
//  This is similar to a Flat naming system
//  1.  except for how names are resolved (some error checking for
//  config errors)
//  2.  attribute operations have their own implementations
//
//  The bindings of junctions
//  are stored in the binding table of the HU context.
//

/* For supporting HU naming systems */
class FNSP_HUContext : public FNSP_nisplusFlatContext
{
private:
	FN_attrset *attr_multi_get_core(const FNSP_Address &,
	    const FN_string &name,
	    const FN_attrset *attrset, FN_status_csvc &cs);

	FN_attrset *attr_get_ids_core(const FNSP_Address &,
	    const FN_string &, FN_status_csvc&);

	unsigned int search_add_attrs(const FNSP_Address &target_context,
	    FN_searchset *matches, const FN_attrset *return_attr_ids);

	unsigned int search_add_refs(FN_searchset *matches);

protected:
	FN_string *my_orgname;
	unsigned my_child_context_type;
	FN_attrset builtin_attrs;	// only identifiers are significant

	FNSP_HUContext(const FN_ref_addr &, const FN_ref &,
	    unsigned child_context_type, unsigned int auth);
	FN_ref *resolve(const FN_string &, unsigned int flags,
	    FN_status_csvc &status);

	int add_new_attrs(const FN_string &, const FN_attrset *,
			    FNSP_Address *, FN_status_csvc &);
	int remove_old_attrs(const FN_string &, FN_status_csvc &,
			    FNSP_Address ** = NULL, FN_attrset ** = NULL);
	int builtin_attr_exists(const FN_string&, const FN_identifier &id,
			const FN_attribute &query, FN_status_csvc&);

	// subclass must provide implementation for these
	virtual FNSP_Address *get_attribute_context(const FN_string &,
	    unsigned &status, unsigned int local_auth = 0) = 0;
	virtual int check_for_config_error(const FN_string &,
	    FN_status_csvc &) = 0;


	// operations on builtin attributes
	virtual FN_attribute *builtin_attr_get(const FN_string &,
	    const FN_identifier &, FN_status_csvc &) = 0;
	virtual FN_attrset *builtin_attr_get_all(const FN_string &,
	    FN_status_csvc &) = 0;
	virtual FN_searchset *builtin_attr_search(const FN_attrset &,
	    FN_status_csvc &) = 0;

public:
	virtual ~FNSP_HUContext();

	FNSP_HUContext(const FN_string &,
	    unsigned context_type, unsigned child_context_type,
	    unsigned int auth);

	FNSP_HUContext(const FN_ref &, unsigned child_context_type,
	    unsigned int auth);

	FN_namelist* c_list_names(const FN_string &name, FN_status_csvc&);
	FN_bindinglist* c_list_bindings(const FN_string &name,
	    FN_status_csvc&);

	// Naming Operations that affect attributes
	int c_attr_bind(const FN_string &name,
			const FN_ref &ref,
			const FN_attrset *attrs,
			unsigned int exclusive,
			FN_status_csvc &cstat);

	int c_unbind(const FN_string &name, FN_status_csvc &cstat);

	int c_rename(const FN_string &name, const FN_composite_name &,
	    unsigned BindFlags, FN_status_csvc&);

	int c_destroy_subcontext(const FN_string &name, FN_status_csvc &cstat);

	// Attribute Operations
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
	FN_searchlist *c_attr_search(
	    const FN_string &name,
	    const FN_attrset *match_attrs,
	    unsigned int return_ref,
	    const FN_attrset *return_attr_ids,
	    FN_status_csvc &status);

	// Creation routines
	FN_ref *c_create_subcontext(const FN_string &name,
	    FN_status_csvc&);

	FN_ref *c_attr_create_subcontext(const FN_string &name,
					    const FN_attrset *attr,
					    FN_status_csvc &status);
};

#endif	/* _FNSP_HUCONTEXT_HH */
