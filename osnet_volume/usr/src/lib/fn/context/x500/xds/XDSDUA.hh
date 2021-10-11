/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_XDSDUA_HH
#define	_XDSDUA_HH

#pragma ident	"@(#)XDSDUA.hh	1.1	96/03/31 SMI"


#include "X500DUA.hh"
#include "XDSXOM.hh"

extern "C" {

#include "xom.h"
#include "xds.h"
#include "xdsbdcp.h"
#include "xdsxfnp.h"

}


/*
 * X.500 Directory User Agent over the XDS API
 */


// macro to call an XDS function from a shared object

#define	CALL_XDS_FUNC(FUNC_RESULT, FUNC_NAME, FUNC_ARGS)		\
{									\
	void	*func;							\
									\
	if (func = get_xomxds_sym(#FUNC_NAME)) {			\
		FUNC_RESULT = (*((x##FUNC_NAME##_p)func))FUNC_ARGS;	\
	} else {							\
		if (strcmp(#FUNC_NAME, "ds_initialize") == 0)		\
			FUNC_RESULT = 0;				\
		else							\
			FUNC_RESULT = DS_NO_WORKSPACE;			\
	}								\
}


class XDSDUA : public X500DUA, public XDSXOM
{

	void			*get_xomxds_sym(const char *func_name) const;

void
x500_dump(
#ifdef DEBUG
	OM_object object
#else
	OM_object
#endif
) const;


	// XDS Function pointers

typedef DS_status		(*xds_abandon_p)(OM_private_object, OM_sint);

typedef DS_status		(*xds_add_entry_p)(OM_private_object,
				    OM_private_object, OM_object, OM_object,
				    OM_sint *);

typedef DS_status		(*xds_bind_p)(OM_object, OM_workspace,
				    OM_private_object *);

typedef DS_status		(*xds_compare_p)(OM_private_object,
				    OM_private_object, OM_object, OM_object,
				    OM_private_object *, OM_sint *);

typedef OM_workspace		(*xds_initialize_p)(void);

typedef DS_status		(*xds_list_p)(OM_private_object,
				    OM_private_object, OM_object,
				    OM_private_object *, OM_sint *);

typedef DS_status		(*xds_modify_entry_p)(OM_private_object,
				    OM_private_object, OM_object, OM_object,
				    OM_sint *);

typedef DS_status		(*xds_modify_rdn_p)(OM_private_object,
				    OM_private_object, OM_object, OM_object,
				    OM_boolean, OM_sint *);

typedef DS_status		(*xds_read_p)(OM_private_object,
				    OM_private_object, OM_object, OM_object,
				    OM_private_object *, OM_sint *);

typedef DS_status		(*xds_receive_result_p)(OM_private_object,
				    OM_uint *, DS_status *, OM_private_object *,
				    OM_sint *);

typedef DS_status		(*xds_remove_entry_p)(OM_private_object,
				    OM_private_object, OM_object, OM_sint *);

typedef DS_status		(*xds_search_p)(OM_private_object,
				    OM_private_object, OM_object, OM_sint,
				    OM_object, OM_boolean, OM_object,
				    OM_private_object *, OM_sint *);

typedef DS_status		(*xds_shutdown_p)(OM_workspace);

typedef DS_status		(*xds_unbind_p)(OM_private_object);

typedef DS_status		(*xds_version_p)(DS_feature [], OM_workspace);



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


	// transferred from XDSXOM class (XDS functions are called)

	FN_ref			*xds_attr_list_to_ref(const FN_string name,
				    OM_public_object attr_list,
				    unsigned int total,
				    OM_private_object context,
				    OM_private_object dn, int &err);

	int			xds_result_to_set(const FN_string &name,
				    OM_private_object result,
				    OM_private_object context,
				    unsigned int return_ref, FN_searchset *set,
				    FN_ext_searchset *ext_set, int &err);


public:

	XDSDUA(int &err);
	virtual ~XDSDUA();
};


#endif	/* _XDSDUA_HH */
