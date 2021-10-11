/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_NISIMPL_HH
#define	_FNSP_NISIMPL_HH

#pragma ident	"@(#)FNSP_nisImpl.hh	1.5	97/10/27 SMI"

#include <FNSP_Impl.hh>
#include "FNSP_nisAddress.hh"

// NIS address routines
extern const FN_identifier &
FNSP_nis_address_type_name(void);

// Function for organizations
extern FN_string *
FNSP_nis_get_org_nns_objname(const FN_string *dirname);

extern FN_string *
FNSP_nis_orgname_of(FN_string &name, unsigned &status);


// Class definition for FNSP_nisImpl
class FNSP_nisImpl:  public FNSP_Impl {
private:
	unsigned add_binding_without_attrs(
	    const FN_string &aname,
	    const FN_ref &ref,
	    unsigned flags);
	FN_ref * create_and_bind_without_attrs(
	    const FN_string &child_name,
	    unsigned context_type,
	    unsigned repr_type,
	    unsigned &status,
	    int find_legal_name = 0,
	    const FN_identifier *ref_type = 0);
	unsigned add_sub_context_entry(const FN_string &);
	unsigned delete_sub_context_entry(const FN_string &);

	FN_searchlist *search_attrset_hu(
	    const FN_attrset *attrset, unsigned int return_ref,
	    const FN_attrset *return_attr_id, unsigned int &status);

	int delete_attrset(const FN_string &atomic_name);
public:
	// constructors
	FNSP_nisImpl(FNSP_nisAddress *addr);
	~FNSP_nisImpl();

	FN_string *get_nns_objname(const FN_string *dirname);
	FN_ref *get_nns_ref();
	int is_this_address_type_p(const FN_ref_addr &addr);

	unsigned add_binding(const FN_string &atomic_name,
	    const FN_ref &ref, const FN_attrset *attrs, unsigned flags);

	unsigned remove_binding(const FN_string &atomic_name);
	unsigned remove_binding(const FN_string &atomic_name, int force);

	unsigned rename_binding(
	    const FN_string &atomic_name,
	    const FN_string &new_name,
	    unsigned flags);

	FN_ref * lookup_binding(const FN_string &atomic_name,
	    unsigned &status);

	unsigned context_exists();

	FN_ref * create_context(unsigned &status,
	    const FN_string *dirname = 0,
	    const FN_identifier *reftype = 0);

	unsigned destroy_context(const FN_string *dirname = 0);

	FN_ref * create_and_bind(
	    const FN_string &child_name,
	    unsigned context_type,
	    unsigned repr_type,
	    unsigned &status,
	    int find_legal_name = 0,
	    const FN_identifier *ref_type = 0,
	    const FN_attrset *attrs = 0);

	unsigned destroy_and_unbind(const FN_string &child_name);

	FN_nameset* list_nameset(unsigned &status, int children_only = 0);
	FN_namelist* list_names(unsigned &status, int children_only = 0);

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

	// Extensions to FNSP_Impl
	int is_org_context();

	FN_namelist *list_names_hu(unsigned &status);

	FN_bindinglist *list_bindings_hu(unsigned &status);
};

#endif	/* _FNSP_NISIMPL_HH */
