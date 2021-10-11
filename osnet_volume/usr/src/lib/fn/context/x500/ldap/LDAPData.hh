/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_LDAPDATA_HH
#define	_LDAPDATA_HH

#pragma ident	"@(#)LDAPData.hh	1.3	96/04/14 SMI"

#include "X500XFN.hh"
#include "lber.h"
#include "ldap.h"

/*
 * LDAP data structure manipulation
 */


enum xfn_set_type {
	XFN_NAME_SET = 1,
	XFN_BINDING_SET,
	XFN_ATTR_SET,
	XFN_SEARCH_SET,
	XFN_EXT_SEARCH_SET
};


class LDAPData : public X500XFN
{

public:

	unsigned char		*string_to_ldap_dn(const FN_string &dn);

	unsigned char		*ldap_dn_to_string(char *name, int is_rdn,
				    int &err);

	int			exclude_matching_rdns(char *parent,
				    char *child);

	int			ldap_error_to_xfn(int &ldap_err);

	FN_ref			*ldap_entry_to_ref(LDAP *ld,
				    unsigned char *name, LDAPMessage *entry,
				    int &err);

	FN_identifier		*ldap_attr_to_ref_type(char **values) const;

	unsigned char		*get_next_ldap_rdn(char *name);

	const unsigned char	*id_to_ldap_attr(const FN_identifier &id,
				    int &err);

	const unsigned char	**ids_to_ldap_attrs(const FN_attrset *ids,
				    char **more_attrs, int &err);

	int			locate_ldap_value(char **values,
				    struct berval *bval);

	void			cleanup_ldap_mods(LDAPMod **mod_list);

	int			mod_op_to_ldap_mod_op(unsigned mod_op,
				    int &ldap_mod_op, char **values, int &err);

	LDAPMod			**mods_to_ldap_mods(const FN_attrmodlist &mods,
				    LDAP *ld, LDAPMessage *entry, int &err);

	unsigned char		*attrs_to_ldap_filter(
				    const FN_attrset *match_attrs, int &err);

	int			extract_search_controls(LDAP *ld,
				    const FN_search_control *control,
				    int &scope, int &follow_links,
				    unsigned int &return_ref, char **&selection,
				    char *select_ref_attrs[], int &err);

	int			ldap_entries_to_set(LDAP *ld,
				    LDAPMessage *entries, FN_nameset *set);

	int			ldap_entries_to_set(LDAP *ld,
				    LDAPMessage *entries, FN_bindingset *set);

	int			ldap_entries_to_set(LDAP *ld,
				    LDAPMessage *entries, FN_attrset *set);

	int			ldap_entries_to_set(LDAP *ld,
				    LDAPMessage *entries,
				    unsigned int return_ref, FN_searchset *set);

	int			ldap_entries_to_set(LDAP *ld, char *base_dn,
				    LDAPMessage *entries,
				    unsigned int return_ref,
				    FN_ext_searchset *set);

	int			ldap_entries_to_set2(LDAP*ld, char *base_dn,
				    LDAPMessage *entries,
				    unsigned int return_ref,
				    enum xfn_set_type set_type,
				    FN_nameset *name_set,
				    FN_bindingset *binding_set,
				    FN_attrset *attr_set,
				    FN_searchset *search_set,
				    FN_ext_searchset *ext_search_set);


	LDAPData() {};
	virtual ~LDAPData() {};
};


#endif	/* _LDAPDATA_HH */
