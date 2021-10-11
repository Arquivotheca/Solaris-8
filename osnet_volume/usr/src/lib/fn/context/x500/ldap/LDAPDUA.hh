/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_LDAPDUA_HH
#define	_LDAPDUA_HH

#pragma ident	"@(#)LDAPDUA.hh	1.2	96/04/02 SMI"

#include "X500DUA.hh"
#include "LDAPData.hh"
#include "lber.h"
#include "ldap.h"


/*
 * X.500 Directory User Agent over the LDAP API
 */


class LDAPDUA : public X500DUA, public LDAPData
{

	// define virtual functions from X500DUA

	FN_ref			*lookup(const FN_string &name,
				    unsigned int authoritative, int &err);

	FN_ref			*lookup_next(const FN_string &name,
				    unsigned int authoritative, int &err);

	FN_namelist		*list_names(const FN_string &name,
				    unsigned int authoritative, int &err);

	FN_bindinglist		*list_bindings(const FN_string &name,
				    unsigned int authoritative, int &err);

	int			bind_next(const FN_string &name,
				    const FN_ref &ref,
				    unsigned int exclusive);

	int			unbind(const FN_string &name);

	int			unbind_next(const FN_string &name);

	int			rename(const FN_string &name,
				    const FN_string *newname,
				    unsigned int exclusive);

	FN_attribute		*get_attr(const FN_string &name,
				    const FN_identifier &id,
				    unsigned int follow_link,
				    unsigned int authoritative, int &err);

	FN_attrset		*get_attr_ids(const FN_string &name,
				    unsigned int follow_link,
				    unsigned int authoritative, int &err);

	FN_multigetlist		*get_attrs(const FN_string &name,
				    const FN_attrset *ids,
				    unsigned int follow_link,
				    unsigned int authoritative, int &err);

	int			modify_attrs(const FN_string &name,
				    const FN_attrmodlist &mods,
				    unsigned int follow_link,
				    FN_attrmodlist **unexmods);

	FN_searchlist		*search_attrs(const FN_string &name,
				    const FN_attrset *match_attrs,
				    unsigned int return_ref,
				    const FN_attrset *attr_ids,
				    unsigned int authoritative, int &err);

	FN_ext_searchlist	*search_attrs_ext(const FN_string &name,
				    const FN_search_control *control,
				    const FN_search_filter *sfilter,
				    unsigned int authoritative, int &err);

	int			bind_attrs(const FN_string &name,
				    const FN_ref *ref, const FN_attrset *attrs,
				    unsigned int exclusive);

public:

	LDAPDUA(int &err);
	virtual ~LDAPDUA();
};


#endif	/* _LDAPDUA_HH */
