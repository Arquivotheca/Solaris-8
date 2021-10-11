/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_X500DUA_HH
#define	_X500DUA_HH

#pragma ident	"@(#)X500DUA.hh	1.1	96/03/31 SMI"


#include <xfn/xfn.h>
#include <xfn/fn_spi.hh>


/*
 * X.500 Directory User Agent
 */


class X500DUA
{

public:
	virtual FN_ref		*lookup(const FN_string &name,
				    unsigned int authoritative, int &err) = 0;

	virtual FN_ref		*lookup_next(const FN_string &name,
				    unsigned int authoritative, int &err) = 0;

	virtual FN_namelist	*list_names(const FN_string &name,
				    unsigned int authoritative, int &err) = 0;

	virtual FN_bindinglist	*list_bindings(const FN_string &name,
				    unsigned int authoritative, int &err) = 0;

	virtual int		bind_next(const FN_string &name,
				    const FN_ref &ref,
				    unsigned int exclusive) = 0;

	virtual int		unbind(const FN_string &name) = 0;

	virtual int		unbind_next(const FN_string &name) = 0;

	virtual int		rename(const FN_string &name,
				    const FN_string *newname,
				    unsigned int exclusive) = 0;

	virtual FN_attribute	*get_attr(const FN_string &name,
				    const FN_identifier &id,
				    unsigned int follow_link,
				    unsigned int authoritative, int &err) = 0;

	virtual FN_attrset	*get_attr_ids(const FN_string &name,
				    unsigned int follow_link,
				    unsigned int authoritative, int &err) = 0;

	virtual FN_multigetlist	*get_attrs(const FN_string &name,
				    const FN_attrset *ids,
				    unsigned int follow_link,
				    unsigned int authoritative, int &err) = 0;

	virtual int		modify_attrs(const FN_string &name,
				    const FN_attrmodlist &mods,
				    unsigned int follow_link,
				    FN_attrmodlist **unexecuted_mods) = 0;

	virtual FN_searchlist	*search_attrs(const FN_string &name,
				    const FN_attrset *match_attrs,
				    unsigned int return_ref,
				    const FN_attrset *attr_ids,
				    unsigned int authoritative, int &err) = 0;
	virtual FN_ext_searchlist
				*search_attrs_ext(const FN_string &name,
				    const FN_search_control *control,
				    const FN_search_filter *sfilter,
				    unsigned int authoritative, int &err) = 0;

	virtual int		bind_attrs(const FN_string &name,
				    const FN_ref *ref, const FN_attrset *attrs,
				    unsigned int exclusive) = 0;


	X500DUA() {};
	virtual ~X500DUA() {};
};


#endif	/* _X500DUA_HH */
