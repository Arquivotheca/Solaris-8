/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_XDSXOM_HH
#define	_XDSXOM_HH

#pragma ident	"@(#)XDSXOM.hh	1.1	96/03/31 SMI"


#include <xfn/xfn.h>
#include <xfn/fn_spi.hh>
#include "XDSInfo.hh"

extern "C" {

#include "xom.h"
#include "xds.h"
#include "xdsbdcp.h"
#include "xdsxfnp.h"

}


/*
 * XDS/XOM data structure manipulation
 */


class XDSXOM : public XDSInfo
{

protected:

	static OM_private_object	auth_context;
	static OM_boolean		xfn_pkg;

	static const int		max_paddr_length;
	static const int		max_naddr_length;
	static const int		max_post_length;


public:

	FN_identifier		*xds_attr_to_ref_type(
				    OM_public_object obj_class) const;

	FN_ref			*xds_attr_value_to_ref(OM_object attr_value,
				    FN_attrset **attrs, int &ref_err) const;

	int			ref_to_xds_attr_value(const FN_ref *ref,
				    OM_object attr_value) const;

	int			id_to_om_oid(const FN_identifier &id,
				    OM_object_identifier &oid,
				    OM_syntax *syntax,
				    OM_object_identifier **class_oid) const;

	OM_private_object	id_to_xds_selection(const FN_identifier	&id)
				    const;

	OM_private_object	ids_to_xds_selection(const FN_attrset *ids,
				    int add_obj_class, int &ok) const;

	int			string_to_xds_attr_value(FN_string *val_str,
				    const OM_syntax syntax,
				    OM_object_identifier *class_oid,
				    OM_descriptor *attr_value) const;

	void			delete_xds_attr_value(OM_descriptor *attr_value)
				    const;

	unsigned char		*xds_attr_value_to_string(
				    OM_descriptor *attr_value,
				    unsigned int &format, unsigned int &length)
				    const;

	int			xds_error_to_int(OM_private_object &status)
				    const;

	int			xds_error_to_xfn(OM_private_object &status)
				    const;

	OM_public_object	string_to_xds_paddr(const FN_string &paddress)
				    const;

	unsigned char		*string_to_xds_paddr_selector(
				    unsigned char *string, OM_string *selector)
				    const;

	unsigned char 		*xds_paddr_to_string(OM_private_object paddress,
				    int &len) const;

	unsigned char 		*xds_paddr_selector_to_string(
				    OM_string *selector, unsigned char *string)
				    const;

	OM_public_object	string_to_xds_post_addr(const FN_string &post)
				    const;

	unsigned char		*xds_post_addr_to_string(OM_private_object addr,
				    int &len) const;

	FN_attrset		*convert_xds_attr_list(OM_public_object entry,
				    FN_ref *ref) const;

	OM_private_object	attrs_to_xds_entry_mod_list(
				    const FN_attrset *attrs,
				    const FN_ref *ref, int &err) const;

	OM_private_object	mods_to_xds_list(const FN_attrmodlist &mods,
				    OM_public_object attr_list,
				    unsigned int attr_count, int &err) const;

	OM_private_object	locate_xds_attribute(OM_string *oid,
				    OM_public_object attr_list, int attr_num)
				    const;

	int			locate_xds_attribute_value(OM_descriptor *value,
				    FN_string *value_string,
				    OM_descriptor *value_list) const;

	OM_private_object	attrs_to_xds_filter(const FN_attrset *attrs,
				    int &ok) const;

	OM_public_object	string_to_xds_filter_item(
				    const unsigned char *lfilter, int &err)
				    const;

	OM_public_object	string_to_xds_filter(
				    const unsigned char *lfilter,
				    OM_public_object xfilter, int &err) const;

	OM_private_object	filter_to_xds_filter(
				    const FN_search_filter *filter, int &err);

	int			extract_search_controls(
				    const FN_search_control *control,
				    unsigned int authoritative,
				    OM_private_object &new_context,
				    OM_sint &scope,
				    OM_private_object &selection,
				    unsigned int &return_ref, int &err) const;

	XDSXOM() {};
	virtual ~XDSXOM() {};
};


#endif	/* _XDSXOM_HH */
