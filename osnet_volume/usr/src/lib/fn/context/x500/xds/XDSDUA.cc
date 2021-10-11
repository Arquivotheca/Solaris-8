/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)XDSDUA.cc	1.2	97/11/12 SMI"


#include <string.h>
#include <dlfcn.h>	// dlopen(), dlsym()
#include "XDSDUA.hh"
#include "XDSExtern.hh"
#include "XDSDN.hh"


/*
 * X.500 Directory User Agent over the XDS API
 */


// definitions of useful OM objects (read-only)

static DS_feature	packages[] = {
	{OM_STRING(OMP_O_DS_BASIC_DIR_CONTENTS_PKG), OM_TRUE},
	{OM_STRING(OMP_O_DS_XFN_PKG), OM_TRUE},
	{{0, 0}, OM_FALSE}
};

static OM_descriptor	auth_context_mod[] = {
	{DS_DONT_USE_COPY, OM_S_BOOLEAN, {{OM_TRUE, NULL}}},
	OM_NULL_DESCRIPTOR
};

static OM_descriptor	select_all_types[] = {
	OM_OID_DESC(OM_CLASS, DS_C_ENTRY_INFO_SELECTION),
	{DS_ALL_ATTRIBUTES, OM_S_BOOLEAN, {{OM_TRUE, NULL}}},
	{DS_INFO_TYPE, OM_S_ENUMERATION, {{DS_TYPES_ONLY, NULL}}},
	OM_NULL_DESCRIPTOR
};

static OM_descriptor	select_ref_attrs[] = {
	OM_OID_DESC(OM_CLASS, DS_C_ENTRY_INFO_SELECTION),
	{DS_ALL_ATTRIBUTES, OM_S_BOOLEAN, {{OM_FALSE, NULL}}},
	OM_OID_DESC(DS_ATTRIBUTES_SELECTED, DS_A_OBJECT_REF_STRING),
	// OM_OID_DESC(DS_ATTRIBUTES_SELECTED, DS_A_OBJECT_REF_IDENT),
	// OM_OID_DESC(DS_ATTRIBUTES_SELECTED, DS_A_OBJECT_REF_ADDRESSES),
	OM_OID_DESC(DS_ATTRIBUTES_SELECTED, DS_A_OBJECT_CLASS),
	OM_OID_DESC(DS_ATTRIBUTES_SELECTED, DS_A_PRESENTATION_ADDRESS),
	{DS_INFO_TYPE, OM_S_ENUMERATION, {{DS_TYPES_AND_VALUES, NULL}}},
	OM_NULL_DESCRIPTOR
};

static OM_descriptor	select_ref_next[] = {
	OM_OID_DESC(OM_CLASS, DS_C_ENTRY_INFO_SELECTION),
	{DS_ALL_ATTRIBUTES, OM_S_BOOLEAN, {{OM_FALSE, NULL}}},
	OM_OID_DESC(DS_ATTRIBUTES_SELECTED, DS_A_NNS_REF_STRING),
	// OM_OID_DESC(DS_ATTRIBUTES_SELECTED, DS_A_NNS_REF_IDENT),
	// OM_OID_DESC(DS_ATTRIBUTES_SELECTED, DS_A_NNS_REF_ADDRESSES),
	{DS_INFO_TYPE, OM_S_ENUMERATION, {{DS_TYPES_AND_VALUES, NULL}}},
	OM_NULL_DESCRIPTOR
};

static OM_descriptor	remove_ref[] = {
	OM_OID_DESC(OM_CLASS, DS_C_ENTRY_MOD),
	{DS_MOD_TYPE, OM_S_ENUMERATION, {{DS_REMOVE_ATTRIBUTE, NULL}}},
	OM_OID_DESC(DS_ATTRIBUTE_TYPE, DS_A_NNS_REF_STRING),
	OM_NULL_DESCRIPTOR
};

static OM_descriptor	mod_list_remove[] = {
	OM_OID_DESC(OM_CLASS, DS_C_ENTRY_MOD_LIST),
	{DS_CHANGES, OM_S_OBJECT, {{0, remove_ref}}},
	OM_NULL_DESCRIPTOR
};

// XDS/XOM shared object
#if defined(__sparcv9)
static char	*LIBXOMXDS_SO = "/opt/SUNWxds/lib/sparcv9/libxomxds.so";
#else
static char	*LIBXOMXDS_SO = "/opt/SUNWxds/lib/libxomxds.so";
#endif


XDSDUA::XDSDUA(
	int	&err
)
{
	DS_status	status = DS_SUCCESS;
	OM_return_code	om_status;

	if (initialized) {
		initialized++;

		x500_trace("[XDSDUA] already connected to DSA\n");

		return;
	}

	CALL_XDS_FUNC(workspace, ds_initialize, ())

	if (workspace != NULL) {

		CALL_XDS_FUNC(status, ds_version, (packages, workspace))

		if (status == DS_SUCCESS) {

			if (packages[1].activated == OM_TRUE)
				xfn_pkg = OM_TRUE;
			else
				xfn_pkg = OM_FALSE;

			CALL_XDS_FUNC(status, ds_bind, (DS_DEFAULT_SESSION,
			    workspace, &session))

			// create XDS context for authoritative ops
			if ((om_status = om_create(DS_C_CONTEXT, OM_TRUE,
			    workspace, &auth_context)) == OM_SUCCESS) {

				if ((om_status = om_put(auth_context,
				    OM_REPLACE_ALL, auth_context_mod, 0, 0, 0))
				    != OM_SUCCESS) {
					om_delete(auth_context);
					auth_context = 0;
				}
			}
		}
	}
	if (workspace && (status == DS_SUCCESS) && (om_status == OM_SUCCESS)) {
		initialized++;

		x500_trace("[XDSDUA] XDS/XOM initialized OK\n");
		x500_trace("[XDSDUA] connected to DSA\n");

	} else {

		x500_trace("[XDSDUA] XDS/XOM initialization error\n");

		err = 1;
		if ((status != DS_SUCCESS) && (status != DS_NO_WORKSPACE)) {
			om_delete(status);
			x500_trace("[XDSDUA] cannot connect to DSA\n");
		}
	}
}


XDSDUA::~XDSDUA(
)
{
	DS_status	status = DS_SUCCESS;
	DS_status	status2 = DS_SUCCESS;

	if (initialized == 1) {

		if (auth_context) {
			auth_context = 0;
		}
		if (session) {

			CALL_XDS_FUNC(status, ds_unbind, (session))

			if (status == DS_SUCCESS)
				session = 0;
		}
		if (workspace) {

			CALL_XDS_FUNC(status2, ds_shutdown, (workspace))

			if (status2 == DS_SUCCESS)
				workspace = 0;
		}
		if ((status != DS_SUCCESS) || (status2 != DS_SUCCESS)) {

			x500_trace("[~XDSDUA] XDS/XOM shutdown error\n");

			if (status)
				om_delete(status);
			if (status2)
				om_delete(status2);
		} else {

			x500_trace("[~XDSDUA] disconnected from DSA\n");
			x500_trace("[~XDSDUA] XDS/XOM shutdown OK\n");
		}
	} else {

		x500_trace("[~XDSDUA]\n");
	}
	initialized--;
}


/*
 * If the specified entry exists then return a reference to it.
 */
FN_ref *
XDSDUA::lookup(
	const FN_string	&name,
	unsigned int	authoritative,
	int		&err
)
{
	x500_trace("XDSDUA::lookup(\"%s\")\n", name.str());

	XDSDN			xdn(name);
	DS_status		status;
	OM_private_object	context;
	OM_private_object	dn = xdn.internal();
	OM_private_object	result;
	OM_sint			invoke_id;

	context = authoritative ? auth_context : DS_DEFAULT_CONTEXT;

	x500_trace("XDSDUA::lookup: ds_search context: ...\n"),
	x500_dump(context);

	x500_trace("XDSDUA::lookup: ds_search dn: ...\n"),
	x500_dump(dn);

	// test for reference attributes

	CALL_XDS_FUNC(status, ds_search, (session, context, dn, DS_BASE_OBJECT,
	    DS_NO_FILTER, OM_TRUE, select_ref_attrs, &result, &invoke_id))

	if (status != DS_SUCCESS) {

		x500_trace("XDSDUA::lookup: ds_search status: ...\n"),
		x500_dump(status);

		err = xds_error_to_xfn(status);
		om_delete(status);
		return (0);
	}
	x500_trace("XDSDUA::lookup: ds_search result: ...\n"),
	x500_dump(result);

	// extract reference attributes
	OM_type			route[] = {DS_SEARCH_INFO, DS_ENTRIES, 0};
	OM_type			type[] = {DS_ATTRIBUTES, 0};
	OM_public_object	pub_result;
	OM_value_position	total;

	if (deep_om_get(route, result, OM_EXCLUDE_ALL_BUT_THESE_TYPES, type,
	    OM_FALSE, 0, 0, &pub_result, &total) != OM_SUCCESS) {

		om_delete(result);
		err = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	om_delete(result);

	FN_ref	*ref;

	if (! (ref =
	    xds_attr_list_to_ref(name, pub_result, (unsigned int) total,
	    context, dn, err))) {
		om_delete(pub_result);
		return (0);
	}
	om_delete(pub_result);
	err = FN_SUCCESS;

	return (ref);
}


/*
 * If the specified entry holds a reference then return that reference.
 */
FN_ref *
XDSDUA::lookup_next(
	const FN_string	&name,
	unsigned int	authoritative,
	int		&err
)
{
	x500_trace("XDSDUA::lookup_next(\"%s\")\n", name.str());

	XDSDN			xdn(name);
	DS_status		status;
	OM_private_object	context;
	OM_private_object	dn = xdn.internal();
	OM_private_object	result;
	OM_sint			invoke_id;

	context = authoritative ? auth_context : DS_DEFAULT_CONTEXT;

	x500_trace("XDSDUA::lookup_next: ds_search context: ...\n"),
	x500_dump(context);

	x500_trace("XDSDUA::lookup_next: ds_search dn: ...\n"),
	x500_dump(dn);

	// test for external reference attribute

	CALL_XDS_FUNC(status, ds_search, (session, context, dn, DS_BASE_OBJECT,
	    DS_NO_FILTER, OM_TRUE, select_ref_next, &result, &invoke_id))

	if (status != DS_SUCCESS) {

		x500_trace("XDSDUA::lookup_next: ds_search status: ...\n"),
		x500_dump(status);

		err = xds_error_to_xfn(status);
		om_delete(status);
		return (0);
	}
	x500_trace("XDSDUA::lookup_next: ds_search result: ...\n"),
	x500_dump(result);

	// extract reference attributes
	OM_type			route[] = {DS_SEARCH_INFO, DS_ENTRIES, 0};
	OM_type			type[] = {DS_ATTRIBUTES, 0};
	OM_public_object	pub_result;
	OM_value_position	total;

	if (deep_om_get(route, result, OM_EXCLUDE_ALL_BUT_THESE_TYPES, type,
	    OM_FALSE, 0, 0, &pub_result, &total) != OM_SUCCESS) {

		om_delete(result);
		err = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	om_delete(result);

	if (total == 0) {
		err = FN_E_NAME_NOT_FOUND;
		return (0);
	}

	// build reference
	FN_ref			*ref = 0;
	FN_attrset		*attrs = 0;
	OM_public_object	attr = pub_result->value.object.object;
	OM_public_object	attr2 = ++attr;	// skip OM_CLASS
	int			fetch_attrs = 0;
	int			ref_err;

	while (attr->type != DS_ATTRIBUTE_TYPE &&
	    attr->type != OM_NO_MORE_TYPES)
		attr++;

	// expects a single attribute
	if (attr->type == DS_ATTRIBUTE_TYPE) {

		if (compare_om_oids(DS_A_NNS_REF_STRING,
		    attr->value.string)) {

			x500_trace("XDSDUA::lookup_next: %s\n",
			    "string-reference attribute encountered");

			while (attr2->type != DS_ATTRIBUTE_VALUES &&
			    attr2->type != OM_NO_MORE_TYPES)
				attr2++;

			if ((attr2->type != DS_ATTRIBUTE_VALUES) ||
			    (! (ref = xds_attr_value_to_ref(attr2, &attrs,
			    ref_err)))) {
				om_delete(pub_result);
				err = ref_err;
				return (0);
			}
			if (attrs) {
				// %%% retrieve additional attributes
				// %%% append attributes to reference

				x500_trace("XDSDUA::lookup_next: %s\n",
				    "additional reference attributes ignored");

				delete attrs;
			}
		} else {

			x500_trace("XDSDUA::lookup_next: %s\n",
			    "unexpected attribute encountered");

			om_delete(pub_result);
			err = FN_E_OPERATION_NOT_SUPPORTED;
			return (0);
		}
	}
	om_delete(pub_result);

	err = FN_SUCCESS;
	return (ref);
}

/*
 * Query the specified entry for its subordinates using ds_list()
 */
FN_namelist *
XDSDUA::list_names(
	const FN_string	&name,
	unsigned int	authoritative,
	int		&err
)
{
	x500_trace("XDSDUA::list_names(\"%s\")\n", name.str());

	XDSDN			xdn(name);
	DS_status		status;
	OM_private_object	context;
	OM_private_object	dn = xdn.internal();
	OM_private_object	result;
	OM_sint			invoke_id;

	context = authoritative ? auth_context : DS_DEFAULT_CONTEXT;

	x500_trace("XDSDUA::list_names: ds_list context: ...\n"),
	x500_dump(context);

	x500_trace("XDSDUA::list_names: ds_list dn: ...\n"),
	x500_dump(dn);

	// locate the children of the specified entry

	CALL_XDS_FUNC(status, ds_list, (session, context, dn, &result,
	    &invoke_id))

	if (status != DS_SUCCESS) {

		x500_trace("XDSDUA::list_names: ds_list status: ...\n"),
		x500_dump(status);

		err = xds_error_to_xfn(status);
		om_delete(status);
		return (0);
	}

	x500_trace("XDSDUA::list_names: ds_list result: ...\n"),
	x500_dump(result);

	// extract subordinates
	OM_type			route[] = {DS_LIST_INFO, 0};
	OM_type			type[] = {DS_SUBORDINATES, 0};
	OM_public_object	pub_result = 0;
	OM_value_position	total;
	OM_public_object	rdn;

	if (deep_om_get(route, result, OM_EXCLUDE_ALL_BUT_THESE_TYPES, type,
	    OM_FALSE, 0, 0, &pub_result, &total) != OM_SUCCESS) {

		om_delete(result);
		err = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	om_delete(result);

	// build set of names
	FN_nameset	*name_set;
	unsigned char	*cp;

	x500_trace("XDSDUA::list_names: found %d entr%s\n", total,
	    (total == 1) ? "y" : "ies");

	if ((name_set = new FN_nameset) == 0) {
		om_delete(pub_result);
		err = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}

	if (total == 0) {
		err = FN_SUCCESS;
		return (new FN_namelist_svc(name_set));
	}

	while (total--) {
		rdn = pub_result[total].value.object.object;
		while (rdn->type != DS_RDN)
			rdn++;

		if (cp = XDSDN(rdn->value.object.object).str()) {
			name_set->add(cp);
			delete [] cp;
		} else {
			err = FN_E_INSUFFICIENT_RESOURCES;
			om_delete(pub_result);
			return (0);
		}

	}

	om_delete(pub_result);
	err = FN_SUCCESS;
	return (new FN_namelist_svc(name_set));
}


/*
 * Query the specified entry for its subordinates (and their reference
 * attributes) using ds_search()
 */
FN_bindinglist *
XDSDUA::list_bindings(
	const FN_string	&name,
	unsigned int	authoritative,
	int		&err
)
{
	x500_trace("XDSDUA::list_bindings(\"%s\")\n", name.str());

	XDSDN			xdn(name);
	DS_status		status;
	OM_private_object	context;
	OM_private_object	dn = xdn.internal();
	OM_private_object	result;
	OM_sint			invoke_id;

	context = authoritative ? auth_context : DS_DEFAULT_CONTEXT;

	x500_trace("XDSDUA::list_bindings: ds_search context: ...\n"),
	x500_dump(context);

	x500_trace("XDSDUA::list_bindings: ds_search dn: ...\n"),
	x500_dump(dn);

	// locate the childern (and their attributes) of the specified entry

	CALL_XDS_FUNC(status, ds_search, (session, context, dn, DS_ONE_LEVEL,
	    DS_NO_FILTER, OM_TRUE, select_ref_attrs, &result, &invoke_id))

	if (status != DS_SUCCESS) {

		x500_trace("XDSDUA::list_bindings: ds_search status: ...\n"),
		x500_dump(status);

		err = xds_error_to_xfn(status);
		om_delete(status);
		return (0);
	}

	x500_trace("XDSDUA::list_bindings: ds_search result: ...\n"),
	x500_dump(result);

	// extract reference attributes
	OM_type			route[] = {DS_SEARCH_INFO, 0};
	OM_type			type[] = {DS_ENTRIES, 0};
	OM_public_object	pub_result = 0;
	OM_value_position	total;

	if (deep_om_get(route, result, OM_EXCLUDE_ALL_BUT_THESE_TYPES, type,
	    OM_FALSE, 0, 0, &pub_result, &total) != OM_SUCCESS) {

		om_delete(result);
		err = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	om_delete(result);

	// build set of bindings
	FN_bindingset	*binding_set;

	x500_trace("XDSDUA::list_bindings: found %d entr%s\n", total,
	    (total == 1) ? "y" : "ies");

	if ((binding_set = new FN_bindingset) == 0) {
		err = FN_E_INSUFFICIENT_RESOURCES;
		om_delete(pub_result);
		return (0);
	}

	if (total == 0) {
		err = FN_SUCCESS;
		return (new FN_bindinglist_svc(binding_set));
	}

	OM_public_object	entry;
	OM_public_object	entry2;
	OM_public_object	attrs;
	OM_public_object	child;
	OM_public_object	rdn;
	FN_string		*rdn_string;
	FN_string		slash((unsigned char *)"/");
	unsigned char		*cp;

	while (total--) {
		entry2 = entry = pub_result[total].value.object.object;

		while (entry->type != DS_OBJECT_NAME)
			entry++;

		// locate and store the final RDN
		child = rdn = entry->value.object.object;
		rdn++;
		// skip to the final RDN
		while (rdn->type == DS_RDNS)
			rdn++;
		rdn = (--rdn)->value.object.object;

		XDSDN	xdn(rdn);

		if (! (cp = xdn.str())) {
			err = FN_E_INSUFFICIENT_RESOURCES;
			om_delete(pub_result);
			delete binding_set;
			return (0);
		}
		if (! (rdn_string = new FN_string(cp))) {
			err = FN_E_INSUFFICIENT_RESOURCES;
			om_delete(pub_result);
			delete binding_set;
			delete [] cp;
			return (0);
		}
		delete [] cp;

		// build reference
		FN_ref		*ref = 0;
		unsigned int	status;
		FN_string	subordinate(&status, &name, &slash, rdn_string,
				    0);
		unsigned int	attr_num = 0;

		entry = entry2;	// reset to start
		while ((entry->type != DS_ATTRIBUTES) &&
		    (entry->type != OM_NO_MORE_TYPES))
			entry++;

		// count attributes
		attrs = entry;
		while (attrs[attr_num].type == DS_ATTRIBUTES)
			attr_num++;

		if ((! attr_num) ||
		    (! (ref = xds_attr_list_to_ref(subordinate, attrs, attr_num,
		    context, child, err)))) {
			om_delete(pub_result);
			delete binding_set;
			delete rdn_string;
			return (0);
		}
		binding_set->add(*rdn_string, *ref);
		delete rdn_string;
	}
	om_delete(pub_result);
	err = FN_SUCCESS;

	return (new FN_bindinglist_svc(binding_set));
}


/*
 * Add the supplied reference to the specified entry using ds_modify_entry()
 */
int
XDSDUA::bind_next(
	const FN_string	&name,
	const FN_ref	&ref,
	unsigned int	exclusive
)
{
	XDSDN			xdn(name);
	DS_status		status;
	OM_private_object	dn = xdn.internal();
	OM_sint			invoke_id;

	// %%% moved here to satisfy SC2.0.1 compiler
	x500_trace("XDSDUA::bind_next(\"%s\")\n", name.str());

	x500_trace("XDSDUA::bind_next: ds_modify_entry dn: ...\n"),
	x500_dump(dn);

	// build DS_C_ENTRY_MOD object
	OM_descriptor	add_ref[5];
	OM_descriptor	*xref = &add_ref[3];	// reference

	fill_om_desc(add_ref[0], OM_CLASS, DS_C_ENTRY_MOD);
	fill_om_desc(add_ref[1], DS_MOD_TYPE, DS_ADD_ATTRIBUTE,
		OM_S_ENUMERATION);
	fill_om_desc(add_ref[2], DS_ATTRIBUTE_TYPE, DS_A_NNS_REF_STRING);
	fill_om_desc(add_ref[3], DS_ATTRIBUTE_VALUES, (OM_descriptor *)0);
	fill_om_desc(add_ref[4]);

	if (! ref_to_xds_attr_value(&ref, &add_ref[3])) {
		return (FN_E_MALFORMED_REFERENCE);
	}

	// build DS_C_ENTRY_MOD_LIST object
	OM_descriptor	mod_list[4];

	fill_om_desc(mod_list[0], OM_CLASS, DS_C_ENTRY_MOD_LIST);
	if (exclusive) {
		// fail if ref attribute already present
		fill_om_desc(mod_list[1], DS_CHANGES, add_ref);
		fill_om_desc(mod_list[2]);
	} else {
		// overwrite if ref attribute already present
		fill_om_desc(mod_list[1], DS_CHANGES, remove_ref);
		fill_om_desc(mod_list[2], DS_CHANGES, add_ref);
		fill_om_desc(mod_list[3]);
	}

	x500_trace("XDSDUA::bind_next: ds_modify_entry changes: ...\n"),
	x500_dump(mod_list);

	// add the reference attribute

	CALL_XDS_FUNC(status, ds_modify_entry, (session, DS_DEFAULT_CONTEXT, dn,
	    mod_list, &invoke_id))

	if (status != DS_SUCCESS) {

		x500_trace("XDSDUA::bind_next: %s\n",
		    "ds_modify_entry status: ..."),
		x500_dump(status);

		int	xerr = xds_error_to_int(status);

		if ((xerr == DS_E_NO_SUCH_ATTRIBUTE_OR_VALUE) &&
		    (! exclusive)) {
			// ref attribute was not present so add it

			fill_om_desc(mod_list[1], DS_CHANGES, add_ref);
			fill_om_desc(mod_list[2]);

			x500_trace("XDSDUA::bind_next: %s\n",
			    "ds_modify_entry changes: ..."),
			x500_dump(mod_list);

			om_delete(status);

			CALL_XDS_FUNC(status, ds_modify_entry, (session,
			    DS_DEFAULT_CONTEXT, dn, mod_list, &invoke_id))

			if (status != DS_SUCCESS) {

				x500_trace("XDSDUA::bind_next: %s\n",
				    "ds_modify_entry status: ..."),
				x500_dump(status);

				int err = xds_error_to_xfn(status);

				if (xref->syntax == OM_S_OCTET_STRING)
					delete [] xref->value.string.elements;
				om_delete(status);
				return (err);
			}
		} else {
			int	err = xds_error_to_xfn(status);

			if (xref->syntax == OM_S_OCTET_STRING)
				delete [] xref->value.string.elements;
			om_delete(status);
			return (err);
		}
	}

	if (xref->syntax == OM_S_OCTET_STRING)
		delete [] xref->value.string.elements;

	return (FN_SUCCESS);
}


/*
 * Remove the specified entry using ds_remove_entry()
 */
int
XDSDUA::unbind(
	const FN_string	&name
)
{
	x500_trace("XDSDUA::unbind(\"%s\")\n", name.str());

	XDSDN			xdn(name);
	DS_status		status;
	OM_private_object	dn = xdn.internal();
	OM_sint			invoke_id;

	x500_trace("XDSDUA::unbind: ds_remove_entry dn: ...\n"),
	x500_dump(dn);

	// remove the entry

	CALL_XDS_FUNC(status, ds_remove_entry, (session, DS_DEFAULT_CONTEXT, dn,
	    &invoke_id))

	if (status != DS_SUCCESS) {

		x500_trace("XDSDUA::unbind: ds_remove_entry status: ...\n"),
		x500_dump(status);

		int	xerr = xds_error_to_int(status);

		if (xerr == DS_E_NO_SUCH_OBJECT) {

			// if parent entry exists, return success

			OM_public_object	match;
			OM_type			tl[] = {DS_MATCHED, 0};
			OM_value_position	total;
			unsigned char		*match_dn;

			if ((om_get(status, OM_EXCLUDE_ALL_BUT_THESE_TYPES, tl,
			    OM_FALSE, 0, 0, &match, &total) == OM_SUCCESS) &&
			    total) {

				const unsigned char	*leaf_dn = name.str();
				const unsigned char	*cp = leaf_dn;
				int			match_len;

				// exclude final RDN

				if (cp) {
					while (*cp++) {
					}

					while (*cp != '/')
						cp--;	// backup to final slash

					match_len = cp - leaf_dn;
					XDSDN	xdn(match->value.object.object);

					if (match_dn = xdn.str(1)) {

						if ((match_len == 0) ||
						    (strncasecmp(
						    (char *)leaf_dn,
						    (char *)match_dn,
						    match_len) == 0)) {

							x500_trace("%s: %s\n",
							    "XDSDUA::unbind",
							    "parent exists");

							om_delete(status);
							om_delete(match);
							delete [] match_dn;
							return (FN_SUCCESS);
						}
						delete [] match_dn;
					}
				}
				om_delete(match);
			}
		}

		int	err = xds_error_to_xfn(status);

		om_delete(status);
		return (err);
	}

	return (FN_SUCCESS);
}


/*
 * Remove reference from the specified entry using ds_modify_entry()
 */
int
XDSDUA::unbind_next(
	const FN_string	&name
)
{
	x500_trace("XDSDUA::unbind_next(\"%s\")\n", name.str());

	XDSDN			xdn(name);
	DS_status		status;
	OM_private_object	dn = xdn.internal();
	OM_private_object	changes;
	OM_sint			invoke_id;

	x500_trace("XDSDUA::unbind_next: ds_modify_entry dn: ...\n"),
	x500_dump(dn);

	// use a DS_C_ENTRY_MOD_LIST object to remove the reference attribute
	changes = mod_list_remove;

	x500_trace("XDSDUA::unbind_next: ds_modify_entry changes: ...\n"),
	x500_dump(changes);

	// remove the reference attribute

	CALL_XDS_FUNC(status, ds_modify_entry, (session, DS_DEFAULT_CONTEXT, dn,
	    changes, &invoke_id))

	if (status != DS_SUCCESS) {

		x500_trace("XDSDUA::unbind_next: ds_modify_entry %s: ...\n",
		    "status"),
		x500_dump(status);

		int	xerr = xds_error_to_int(status);

		// if no binding present return success
		if (xerr == DS_E_NO_SUCH_ATTRIBUTE_OR_VALUE) {
			om_delete(status);
			return (FN_SUCCESS);
		}

		int	err = xds_error_to_xfn(status);

		om_delete(status);
		return (err);
	}

	return (FN_SUCCESS);
}


/*
 * Rename the specified leaf entry to the supplied name using ds_modify_rdn()
 */
int
XDSDUA::rename(
	const FN_string	&name,
	const FN_string	*newname,
	unsigned int
)
{
	x500_trace("XDSDUA::rename(\"%s\",\"%s\")\n", name.str(),
		newname->str());

	XDSDN			xdn_old(name);
	XDSDN			xdn_new(*newname);
	DS_status		status;
	OM_private_object	dn_old = xdn_old.internal();
	OM_private_object	dn_new = xdn_new.internal();
	OM_object		rdn;
	OM_sint			invoke_id;

	x500_trace("XDSDUA::rename: ds_modify_rdn dn_old: ...\n"),
	x500_dump(dn_old);

	// locate rdn within the dn
	{
		OM_type			tl[] = {DS_RDNS, 0};
		OM_value_position	total = 99;

		if ((om_get(dn_new,
		    OM_EXCLUDE_ALL_BUT_THESE_TYPES + OM_EXCLUDE_SUBOBJECTS, tl,
		    OM_FALSE, 0, 0, &rdn, &total) != OM_SUCCESS) || (! total)) {

			if (! total)
				return (FN_E_ILLEGAL_NAME);
			else
				return (FN_E_INSUFFICIENT_RESOURCES);
		}
	}

	x500_trace("XDSDUA::rename: ds_modify_rdn rdn: ...\n"),
	x500_dump(rdn->value.object.object);

	// rename the specified entry

	CALL_XDS_FUNC(status, ds_modify_rdn, (session, DS_DEFAULT_CONTEXT,
	    dn_old, rdn->value.object.object, OM_TRUE, &invoke_id))

	om_delete(rdn);
	if (status != DS_SUCCESS) {

		x500_trace("XDSDUA::rename: ds_modify_rdn status: ...\n"),
		x500_dump(status);

		int	err = xds_error_to_xfn(status);

		om_delete(status);
		return (err);
	}

	return (FN_SUCCESS);
}


/*
 * Query the specified entry for the requested attribute using ds_read().
 */
FN_attribute *
XDSDUA::get_attr(
	const FN_string		&name,
	const FN_identifier	&id,
	unsigned int,
	unsigned int		authoritative,
	int			&err
)
{
	x500_trace("XDSDUA::get_attr(\"%s\")\n", name.str());

	XDSDN			xdn(name);
	DS_status		status;
	OM_private_object	context;
	OM_private_object	dn = xdn.internal();
	OM_private_object	selection;
	OM_private_object	result;
	OM_sint			invoke_id;

	context = authoritative ? auth_context : DS_DEFAULT_CONTEXT;

	x500_trace("XDSDUA::get_attr: ds_read context: ...\n"),
	x500_dump(context);

	x500_trace("XDSDUA::get_attr: ds_read dn: ...\n"),
	x500_dump(dn);

	if (! (selection = id_to_xds_selection(id))) {
		err = FN_E_INVALID_ATTR_IDENTIFIER;
		return (0);
	}

	x500_trace("XDSDUA::get_attr: ds_read selection: ...\n"),
	x500_dump(selection);

	// retrieve the requested attribute

	CALL_XDS_FUNC(status, ds_read, (session, context, dn, selection,
	    &result, &invoke_id))

	om_delete(selection);
	if (status != DS_SUCCESS) {

		x500_trace("XDSDUA::get_attr: ds_read status: ...\n"),
		x500_dump(status);

		err = xds_error_to_xfn(status);
		om_delete(status);
		return (0);
	}

	x500_trace("XDSDUA::get_attr: ds_read result: ...\n"),
	x500_dump(result);

	// extract requested attribute value(s)
	OM_type			route[] = {DS_ENTRY, DS_ATTRIBUTES, 0};
	OM_type			type[] = {DS_ATTRIBUTE_VALUES, 0};
	OM_public_object	pub_result;
	OM_value_position	total;

	if (deep_om_get(route, result, OM_EXCLUDE_ALL_BUT_THESE_TYPES, type,
	    OM_FALSE, 0, 0, &pub_result, &total) != OM_SUCCESS) {

		om_delete(result);
		err = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	om_delete(result);

	// build attribute
	FN_attribute	*attr = 0;

	x500_trace("XDSDUA::get_attr: retrieved %d attribute value%s\n",
	    total, (total == 1) ? "" : "s");

	if (total == 0) {
		err = FN_SUCCESS;
		return (0);
	}

	OM_string 	*val;
	OM_string	str;
	unsigned int	format;
	unsigned int	length;

	while (total--) {
		val = &pub_result[total].value.string;

		if (! (str.elements = xds_attr_value_to_string(
		    &pub_result[total], format, length))) {

			x500_trace("XDSDUA::get_attr: %s\n",
			    "cannot convert attribute value to string");

			continue;	// ignore it
		}

		const FN_attrvalue	av(str.elements, length);

		delete [] str.elements;	// xds_attr_value_to_string() allocates

		if (! attr) {
			// X.500 attribute value is either ascii or octet string
			attr = new FN_attribute(id,
			    ((format == FN_ID_STRING) ||
			    (format == FN_ID_ISO_OID_STRING) ||
			    (format == FN_ID_DCE_UUID)) ?
			    ascii : octet);
		}
		attr->add(av);
	}
	om_delete(pub_result);
	err = FN_SUCCESS;
	return (attr);
}


/*
 * Query the specified entry for all its attribute types using ds_read().
 */
FN_attrset *
XDSDUA::get_attr_ids(
	const FN_string		&name,
	unsigned int,
	unsigned int		authoritative,
	int			&err
)
{
	x500_trace("XDSDUA::get_attr_ids(\"%s\")\n", name.str());

	XDSDN			xdn(name);
	DS_status		status;
	OM_private_object	context;
	OM_private_object	dn = xdn.internal();
	OM_private_object	result;
	OM_sint			invoke_id;

	context = authoritative ? auth_context : DS_DEFAULT_CONTEXT;

	x500_trace("XDSDUA::get_attr_ids: ds_read context: ...\n"),
	x500_dump(context);

	x500_trace("XDSDUA::get_attr_ids: ds_read dn: ...\n"),
	x500_dump(dn);

	// retrieve requested the attribute

	CALL_XDS_FUNC(status, ds_read, (session, context, dn, select_all_types,
	    &result, &invoke_id))

	if (status != DS_SUCCESS) {

		x500_trace("XDSDUA::get_attr_ids: ds_read status: ...\n"),
		x500_dump(status);

		err = xds_error_to_xfn(status);
		om_delete(status);
		return (0);
	}

	x500_trace("XDSDUA::get_attr_ids: ds_read result: ...\n"),
	x500_dump(result);

	// extract attribute(s)
	OM_type			route[] = {DS_ENTRY, 0};
	OM_type			type[] = {DS_ATTRIBUTES, 0};
	OM_public_object	pub_result;
	OM_value_position	total;

	if (deep_om_get(route, result, OM_EXCLUDE_ALL_BUT_THESE_TYPES, type,
	    OM_FALSE, 0, 0, &pub_result, &total) != OM_SUCCESS) {

		om_delete(result);
		err = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	om_delete(result);

	// build set of attribute ids
	FN_attribute	*attr = 0;
	FN_attrset	*attrs = 0;

	x500_trace("XDSDUA::get_attr_ids: retrieved %d attribute id%s\n",
	    total, (total == 1) ? "" : "s");

	if (total == 0) {
		err = FN_SUCCESS;
		return (0);
	}

	OM_descriptor	*at;
	OM_string 	*val;
	OM_string	str;
	unsigned int	format;
	unsigned int	length;
	OM_syntax	syntax;

	while (total--) {
		at = pub_result[total].value.object.object + 1;
		val = &at->value.string;

		if (! (str.elements = xds_attr_value_to_string(at, format,
		    length))) {

			x500_trace("XDSDUA::get_attr_ids: %s\n",
			    "cannot convert attribute type to string");

			continue;	// ignore it
		}
		const FN_identifier	id(format, length, str.elements);

		delete [] str.elements;	// xds_attr_value_to_string() allocates

		if (! attrs)
			attrs = new FN_attrset;

		if (! om_oid_to_syntax(val, &syntax, 0)) {

			x500_trace("XDSDUA::get_attr_ids: %s\n",
			    "cannot locate attribute's syntax");

			continue;	// ignore it
		}
		// X.500 attribute value is either ascii or octet string
		attrs->add(FN_attribute(id,
		    (syntax != OM_S_OCTET_STRING) ? ascii : octet));
	}
	om_delete(pub_result);
	err = FN_SUCCESS;
	return (attrs);
}


/*
 * Query the specified entry for the requested attributes using ds_read().
 */
FN_multigetlist *
XDSDUA::get_attrs(
	const FN_string		&name,
	const FN_attrset	*ids,
	unsigned int,
	unsigned int		authoritative,
	int			&err
)
{
	x500_trace("XDSDUA::get_attrs(\"%s\")\n", name.str());

	XDSDN			xdn(name);
	DS_status		status;
	OM_private_object	context;
	OM_private_object	dn = xdn.internal();
	OM_private_object	selection;
	OM_private_object	result;
	OM_sint			invoke_id;
	int			ok = 1;

	context = authoritative ? auth_context : DS_DEFAULT_CONTEXT;

	x500_trace("XDSDUA::get_attrs: ds_read context: ...\n"),
	x500_dump(context);

	x500_trace("XDSDUA::get_attrs: ds_read dn: ...\n"),
	x500_dump(dn);

	selection = ids_to_xds_selection(ids, 0, ok);
	if (! ok) {
		err = FN_E_INVALID_ATTR_IDENTIFIER;
		return (0);
	}

	if ((selection != DS_SELECT_ALL_TYPES_AND_VALUES) &&
	    (selection != DS_SELECT_NO_ATTRIBUTES)) {

		x500_trace("XDSDUA::get_attrs: ds_read selection: ...\n"),
		x500_dump(selection);
	}

	// retrieve requested attributes

	CALL_XDS_FUNC(status, ds_read, (session, context, dn, selection,
	    &result, &invoke_id))

	if ((selection != DS_SELECT_ALL_TYPES_AND_VALUES) &&
	    (selection != DS_SELECT_NO_ATTRIBUTES)) {
		om_delete(selection);
	}

	if (status != DS_SUCCESS) {

		x500_trace("XDSDUA::get_attrs: ds_read status: ...\n"),
		x500_dump(status);

		err = xds_error_to_xfn(status);
		om_delete(status);
		return (0);
	}

	x500_trace("XDSDUA::get_attrs: ds_read result: ...\n"),
	x500_dump(result);

	// extract requested attribute value(s)
	OM_type			route[] = {DS_ENTRY, 0};
	OM_type			type[] = {DS_ATTRIBUTES, 0};
	OM_public_object	pub_result;
	OM_value_position	total;

	if (deep_om_get(route, result, OM_EXCLUDE_ALL_BUT_THESE_TYPES, type,
	    OM_FALSE, 0, 0, &pub_result, &total) != OM_SUCCESS) {

		om_delete(result);
		err = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	om_delete(result);

	// build set of attributes
	FN_attrset	*attrs = 0;

	x500_trace("XDSDUA::get_attrs: retrieved %d attribute%s\n",
	    total, (total == 1) ? "" : "s");

	if (total == 0) {
		err = FN_SUCCESS;
		return (0);
	}

	OM_descriptor	*at;
	OM_descriptor	*at2;
	OM_string 	*val;
	OM_string	str;
	FN_identifier	*id;
	FN_attribute	*attr;
	unsigned int	format;
	unsigned int	length;

	while (total--) {
		at2 = at = pub_result[total].value.object.object + 1;
		attr = 0;

		while (at->type != OM_NO_MORE_TYPES) {
			val = &at->value.string;

			if (at->type == DS_ATTRIBUTE_TYPE) {

				if (! (str.elements =
				    xds_attr_value_to_string(at, format,
				    length))) {

					x500_trace("XDSDUA::get_attrs: %s %s",
					    "cannot convert attribute",
					    "type to string\n");

					continue;	// ignore it
				}
				id = new FN_identifier(format, length,
				    str.elements);

				delete [] str.elements;
					// xds_attr_value_to_string() allocates
				break;
			}
			at++;
		}

		at = at2;	// reset
		while (at->type != OM_NO_MORE_TYPES) {
			val = &at->value.string;

			if (at->type == DS_ATTRIBUTE_VALUES) {

				if (! (str.elements =
				    xds_attr_value_to_string(at, format,
				    length))) {

					x500_trace("XDSDUA::get_attrs: %s %s",
					    "cannot convert attribute",
					    "value to string\n");

					continue;	// ignore it
				}
				if (! attr) {
				// X.500 attribute value is either
				//  ascii or octet string
					attr = new FN_attribute(*id,
					    ((format == FN_ID_STRING) ||
					    (format == FN_ID_ISO_OID_STRING) ||
					    (format == FN_ID_DCE_UUID)) ?
					    ascii : octet);
				}
				attr->add(FN_attrvalue(str.elements, length));

				delete [] str.elements;
					// xds_attr_value_to_string() allocates
			}
			at++;
		}
		if (! attrs)
			attrs = new FN_attrset;

		attrs->add(*attr);

		delete attr;
		delete id;
	}

	om_delete(pub_result);
	err = FN_SUCCESS;
	return (new FN_multigetlist_svc(attrs));
}


/*
 * Modify the specified entry using ds_modify_entry() or ds_add_entry().
 */
int
XDSDUA::modify_attrs(
	const FN_string		&name,
	const FN_attrmodlist	&mods,
	unsigned int,
	FN_attrmodlist		**unexmods
)
{
	// %%% remove when XDS supports DS_SELECT_ALL_TYPES_AND_VALUES
	static OM_descriptor	select_all[] = {
		OM_OID_DESC(OM_CLASS, DS_C_ENTRY_INFO_SELECTION),
		{DS_ALL_ATTRIBUTES, OM_S_BOOLEAN, {{OM_TRUE, NULL}}},
		{DS_INFO_TYPE, OM_S_ENUMERATION, {{DS_TYPES_AND_VALUES, NULL}}},
		OM_NULL_DESCRIPTOR
	};

	x500_trace("XDSDUA::modify_attrs(\"%s\")\n", name.str());

	XDSDN			xdn(name);
	DS_status		status;
	OM_private_object	dn = xdn.internal();
	OM_private_object	xds_list;
	OM_private_object	result;
	OM_sint			invoke_id;
	int			err;
	OM_public_object	pub_result = 0;
	OM_value_position	total = 0;

	*unexmods = 0;

	x500_trace("XDSDUA::modify_attrs: ds_modify_entry dn: ...\n"),
	x500_dump(dn);

	x500_trace("XDSDUA::modify_attrs: retrieve X.500 attributes\n");

	CALL_XDS_FUNC(status, ds_read, (session, DS_DEFAULT_CONTEXT, dn,
	    select_all, &result, &invoke_id))

	if (status != DS_SUCCESS) {
		int	xerr = xds_error_to_int(status);

		if (xerr != DS_E_NO_SUCH_OBJECT) {
			err = xds_error_to_xfn(status);
			om_delete(status);
			return (err);
		}
		pub_result = 0;	// not present
		total = 0;

	} else {
		// extract requested attribute value(s)
		OM_type		route[] = {DS_ENTRY, 0};
		OM_type		type[] = {DS_ATTRIBUTES, 0};

		if (deep_om_get(route, result, OM_EXCLUDE_ALL_BUT_THESE_TYPES,
		    type, OM_FALSE, 0, 0, &pub_result, &total) != OM_SUCCESS) {
			om_delete(result);
			return (FN_E_INSUFFICIENT_RESOURCES);
		}
		om_delete(result);

		x500_trace("XDSDUA::modify_attrs: retrieved %d %s\n", total,
		    (total == 1) ? "attribute" : "attributes");
	}
	if (! (xds_list = mods_to_xds_list(mods, pub_result,
	    (unsigned int) total, err))) {
		om_delete(pub_result);
		return (err);
	}
	om_delete(pub_result);

	x500_trace("XDSDUA::modify_attrs: ds_modify_entry xds_list: ...\n");
	x500_dump(xds_list);

	// make the specified modifications

	if (total) {
		CALL_XDS_FUNC(status, ds_modify_entry, (session,
		    DS_DEFAULT_CONTEXT, dn, xds_list, &invoke_id))
	} else {
		CALL_XDS_FUNC(status, ds_add_entry, (session,
		    DS_DEFAULT_CONTEXT, dn, xds_list, &invoke_id))
	}
	om_delete(xds_list);

	if (status != DS_SUCCESS) {

		x500_trace("XDSDUA::modify_attrs: ds_modify_entry %s: ...\n",
		    "status"),
		x500_dump(status);

		err = xds_error_to_xfn(status);
		om_delete(status);
		return (err);
	}
	x500_trace("XDSDUA::modify_attrs: modification(s) completed\n");

	return (FN_SUCCESS);
}


/*
 * Perform a simple search at the specified entry and return the requested
 * attributes using ds_search().
 */
FN_searchlist *
XDSDUA::search_attrs(
	const FN_string		&name,
	const FN_attrset	*match_attrs,
	unsigned int		return_ref,
	const FN_attrset	*attr_ids,
	unsigned int		authoritative,
	int			&err
)
{
	x500_trace("XDSDUA::search_attrs(\"%s\")\n", name.str());

	XDSDN			xdn(name);
	DS_status		status;
	OM_private_object	context;
	OM_private_object	dn = xdn.internal();
	OM_private_object	selection;
	OM_private_object	filter;
	OM_private_object	result;
	OM_sint			invoke_id;
	int			ok = 1;

	context = authoritative ? auth_context : DS_DEFAULT_CONTEXT;

	x500_trace("XDSDUA::search_attrs: ds_search context: ...\n"),
	x500_dump(context);

	x500_trace("XDSDUA::search_attrs: ds_search dn: ...\n"),
	x500_dump(dn);

	selection = ids_to_xds_selection(attr_ids, return_ref, ok);
	if (! ok) {
		err = FN_E_INVALID_ATTR_IDENTIFIER;
		return (0);
	}
	if ((selection != DS_SELECT_ALL_TYPES_AND_VALUES) &&
	    (selection != DS_SELECT_NO_ATTRIBUTES)) {

		x500_trace("XDSDUA::search_attrs: ds_search selection: ...\n"),
		x500_dump(selection);
	}

	// build filter
	filter = attrs_to_xds_filter(match_attrs, ok);
	if (! ok) {
		err = FN_E_SEARCH_INVALID_FILTER;
		return (0);
	}

	x500_trace("XDSDUA::search_attrs: ds_search filter: ...\n"),
	x500_dump(filter);

	// perform search and retrieve requested attributes

	CALL_XDS_FUNC(status, ds_search, (session, context, dn, DS_ONE_LEVEL,
	    filter, OM_TRUE, selection, &result, &invoke_id))

	if ((selection != DS_SELECT_ALL_TYPES_AND_VALUES) &&
	    (selection != DS_SELECT_NO_ATTRIBUTES)) {
		om_delete(selection);
	}
	if (filter != DS_NO_FILTER)
		om_delete(filter);

	if (status != DS_SUCCESS) {

		x500_trace("XDSDUA::search_attrs: ds_search status: ...\n"),
		x500_dump(status);

		err = xds_error_to_xfn(status);
		om_delete(status);
		return (0);
	}

	x500_trace("XDSDUA::search_attrs: ds_search result: ...\n"),
	x500_dump(result);

	// extract entries
	FN_searchset		*set;
	FN_searchlist_svc	*sl;

	if ((set = new FN_searchset) == 0) {
		err = FN_E_INSUFFICIENT_RESOURCES;
		om_delete(result);
		return (0);
	}

	if (xds_result_to_set(name, result, context, return_ref, set, 0, err)) {
		om_delete(result);

		if ((sl = new FN_searchlist_svc(set)) == 0) {
			err = FN_E_INSUFFICIENT_RESOURCES;
			delete set;
			return (0);
		}
		return (sl);
	}
	om_delete(result);
	delete set;
	return (0);
}


/*
 * Perform a filtered search below the specified entry using ds_search().
 */
FN_ext_searchlist *
XDSDUA::search_attrs_ext(
	const FN_string		&name,
	const FN_search_control	*control,
	const FN_search_filter	*sfilter,
	unsigned int		authoritative,
	int			&err
)
{
	x500_trace("XDSDUA::search_attrs_ext(\"%s\")\n", name.str());

	XDSDN			xdn(name);
	DS_status		status;
	OM_private_object	context;
	OM_private_object	new_context = 0;
	OM_private_object	dn = xdn.internal();
	OM_private_object	selection = 0;
	OM_private_object	filter;
	OM_private_object	result;
	OM_sint			scope;
	OM_sint			invoke_id;
	unsigned int		return_ref = 0;
	int			return_attrs = 0;

	if (! (extract_search_controls(control, authoritative, new_context,
	    scope, selection, return_ref, err))) {

		om_delete(new_context);
		if ((selection != DS_SELECT_ALL_TYPES_AND_VALUES) &&
		    (selection != DS_SELECT_NO_ATTRIBUTES)) {
			om_delete(selection);
		}
		return (0);
	}
	context = new_context ? new_context : DS_DEFAULT_CONTEXT;

	x500_trace("XDSDUA::search_attrs_ext: ds_search context: ...\n"),
	x500_dump(context);

	x500_trace("XDSDUA::search_attrs_ext: ds_search dn: ...\n"),
	x500_dump(dn);

	x500_trace("XDSDUA::search_attrs_ext: ds_search scope: %d\n", scope);

	x500_trace("XDSDUA::search_attrs_ext: ds_search selection: ...\n"),
	x500_dump(selection);

	// build filter
	if (! (filter = filter_to_xds_filter(sfilter, err))) {
		return (0);
	}

	x500_trace("XDSDUA::search_attrs_ext: ds_search filter: ...\n"),
	x500_dump(filter);

	// perform search and retrieve requested attributes

	CALL_XDS_FUNC(status, ds_search, (session, context, dn, scope, filter,
	    OM_TRUE, selection, &result, &invoke_id))

	if ((selection != DS_SELECT_ALL_TYPES_AND_VALUES) &&
	    (selection != DS_SELECT_NO_ATTRIBUTES)) {
		om_delete(selection);
	}
	if (new_context)
		om_delete(new_context);
	if (filter != DS_NO_FILTER)
		om_delete(filter);

	if (status != DS_SUCCESS) {

		x500_trace("XDSDUA::search_attrs_ext: ds_search status: ...\n"),
		x500_dump(status);

		err = xds_error_to_xfn(status);
		om_delete(status);
		return (0);
	}

	x500_trace("XDSDUA::search_attrs_ext: ds_search result: ...\n"),
	x500_dump(result);

	// extract entries
	FN_ext_searchset	*ext_set;
	FN_ext_searchlist_svc	*esl;

	if ((ext_set = new FN_ext_searchset) == 0) {
		err = FN_E_INSUFFICIENT_RESOURCES;
		om_delete(result);
		return (0);
	}

	if (xds_result_to_set(name, result, context, return_ref, 0, ext_set,
	    err)) {
		om_delete(result);

		if ((esl = new FN_ext_searchlist_svc(ext_set)) == 0) {
			err = FN_E_INSUFFICIENT_RESOURCES;
			delete ext_set;
			return (0);
		}
		return (esl);
	}
	om_delete(result);
	delete ext_set;
	return (0);
}


/*
 * Create the specified entry with the supplied reference and attributes
 * using ds_add_entry()
 */
int
XDSDUA::bind_attrs(
	const FN_string		&name,
	const FN_ref		*ref,
	const FN_attrset	*attrs,
	unsigned int		exclusive
)
{
	x500_trace("XDSDUA::bind_attrs(\"%s\")\n", name.str());

	XDSDN			xdn(name);
	DS_status		status;
	OM_private_object	dn = xdn.internal();
	OM_private_object	attributes;
	OM_sint			invoke_id;
	int			err;

	x500_trace("XDSDUA::bind_attrs: ds_add_entry dn: ...\n"),
	x500_dump(dn);

	if (! (attributes =
	    attrs_to_xds_entry_mod_list(attrs, ref, err))) {
		return (err);
	}

	x500_trace("XDSDUA::bind_attrs: ds_add_entry attributes: ...\n"),
	x500_dump(attributes);

	// add the attributes

	CALL_XDS_FUNC(status, ds_add_entry, (session, DS_DEFAULT_CONTEXT, dn,
	    attributes, &invoke_id))

	if (status != DS_SUCCESS) {

		x500_trace("XDSDUA::bind_attrs: %s\n",
		    "ds_add_entry status: ..."),
		x500_dump(status);

		int	xerr = xds_error_to_int(status);

		if (xerr == DS_E_ENTRY_ALREADY_EXISTS) {

			om_delete(status);

			if (exclusive) {
				om_delete(attributes);
				return (FN_E_NAME_IN_USE);
			}

			// %%% if attrs is 0 should add ref not replace entry

			// remove existing entry

			CALL_XDS_FUNC(status, ds_remove_entry, (session,
			    DS_DEFAULT_CONTEXT, dn, &invoke_id))

			if (status != DS_SUCCESS) {

				x500_trace("XDSDUA::bind_attrs: %s\n",
				    "ds_remove_entry status: ..."),
				x500_dump(status);

				int err = xds_error_to_xfn(status);

				om_delete(attributes);
				om_delete(status);
				return (err);
			}

			// add new entry

			CALL_XDS_FUNC(status, ds_add_entry, (session,
			    DS_DEFAULT_CONTEXT, dn, attributes, &invoke_id))

			if (status != DS_SUCCESS) {

				x500_trace("XDSDUA::bind_attrs: %s\n",
				    "ds_add_entry status: ..."),
				x500_dump(status);

				int err = xds_error_to_xfn(status);

				om_delete(attributes);
				om_delete(status);
				return (err);
			}

		} else {
			int err = xds_error_to_xfn(status);

			om_delete(attributes);
			om_delete(status);
			return (err);
		}
	}
	om_delete(attributes);

	return (FN_SUCCESS);
}


/*
 * Convert an XDS attribute list object to an XFN reference.
 *
 * NOTE: additional attributes are fetched when necessary
 */
FN_ref *
XDSDUA::xds_attr_list_to_ref(
	const FN_string		name,
	OM_public_object	attr_list,
	unsigned int		total,
	OM_private_object	context,
	OM_private_object	dn,
	int			&err
)
{
	FN_ref			*ref = 0;
	FN_ref_addr		x500_ref_addr(x500, name.bytecount(),
				    name.str());
	FN_identifier		*id = 0;
	FN_attrset		*attrs = 0;
	const FN_identifier	*rid = 0;
	OM_public_object	attr;
	OM_public_object	attr2;
	unsigned int		attr_num = total;
	int			ref_err;

	// use objectReferenceString attribute (if present) to build reference
	while ((attr_num--) && (! ref)) {
		attr = attr_list[attr_num].value.object.object;

		while ((attr->type != DS_ATTRIBUTE_TYPE) &&
		    (attr->type != OM_NO_MORE_TYPES))
			attr++;

		if (attr->type == DS_ATTRIBUTE_TYPE) {

			if (compare_om_oids(DS_A_OBJECT_REF_STRING,
			    attr->value.string)) {

				attr2 = attr_list[attr_num].value.object.object;
				while ((attr2->type != DS_ATTRIBUTE_VALUES) &&
				    (attr2->type != OM_NO_MORE_TYPES))
					attr2++;

				if (attr2->type == DS_ATTRIBUTE_VALUES) {

					if (! (ref = xds_attr_value_to_ref(
					    attr2, &attrs, ref_err))) {
						err = ref_err;
						return (0);
					}
					rid = ref->type();
				}
			}
		}
	}
	if (! ref) {
		// use objectClass attribute (if present) to set reference type
		attr_num = total;
		while ((attr_num--) && (! id)) {
			attr = attr_list[attr_num].value.object.object;

			while ((attr->type != DS_ATTRIBUTE_TYPE) &&
			    (attr->type != OM_NO_MORE_TYPES))
				attr++;

			if (attr->type == DS_ATTRIBUTE_TYPE) {

				if (compare_om_oids(DS_A_OBJECT_CLASS,
				    attr->value.string)) {
					id = xds_attr_to_ref_type(attr);
				}
			}
		}
		if (! id)
			id = new FN_identifier((const unsigned char *)"x500");

		if ((ref = new FN_ref(*id)) == 0) {
			delete id;
			err = FN_E_INSUFFICIENT_RESOURCES;
			return (0);
		}
	} else {
		if (attrs) {	// retrieve additional attributes

			OM_private_object	selection;
			OM_private_object	result;
			OM_private_object	status;
			OM_sint			invoke_id;
			int			ok = 1;

			selection = ids_to_xds_selection(attrs, 0, ok);
			if (! ok) {
				delete id;
				delete attrs;
				delete ref;
				err = FN_E_MALFORMED_REFERENCE;
				return (0);
			}

			if ((selection != DS_SELECT_ALL_TYPES_AND_VALUES) &&
			    (selection != DS_SELECT_NO_ATTRIBUTES)) {

				x500_trace("XDSXOM::xds_attr_list_to_ref: %s\n",
				    "ds_read selection: ..."),
				x500_dump(selection);
			}

			// retrieve requested attributes

			CALL_XDS_FUNC(status, ds_read, (session, context, dn,
			    selection, &result, &invoke_id))

			if ((selection != DS_SELECT_ALL_TYPES_AND_VALUES) &&
			    (selection != DS_SELECT_NO_ATTRIBUTES)) {
				om_delete(selection);
			}
			delete attrs;

			if (status != DS_SUCCESS) {

				x500_trace("XDSXOM::xds_attr_list_to_ref: %s\n",
				    "ds_read status: ..."),
				x500_dump(status);

				om_delete(status);
				delete id;
				delete ref;
				err = FN_E_MALFORMED_REFERENCE;
				return (0);
			}

			// extract requested attribute value(s)
			OM_type			route[] = {DS_ENTRY, 0};
			OM_type			type[] = {DS_ATTRIBUTES, 0};
			OM_public_object	pub_result;
			OM_value_position	total2;

			if (deep_om_get(route, result,
			    OM_EXCLUDE_ALL_BUT_THESE_TYPES, type, OM_FALSE, 0,
			    0, &pub_result, &total2) != OM_SUCCESS) {
				om_delete(result);
				delete id;
				delete ref;
				err = FN_E_INSUFFICIENT_RESOURCES;
				return (0);
			}
			om_delete(result);

			x500_trace("XDSXOM::xds_attr_list_to_ref: %s %d %s\n",
			    "retrieved", total2,
			    (total2 == 1) ? "attribute" : "attributes");

			// append attributes to reference
			convert_xds_attr_list(pub_result, ref);

			om_delete(pub_result);
		}
	}
	ref->append_addr(x500_ref_addr);

	// append presentationAddress attribute (if present) to reference
	attr_num = total;
	while (attr_num--) {
		attr = attr_list[attr_num].value.object.object;

		while ((attr->type != DS_ATTRIBUTE_TYPE) &&
		    (attr->type != OM_NO_MORE_TYPES))
			attr++;

		if (attr->type == DS_ATTRIBUTE_TYPE) {

			if (compare_om_oids(DS_A_PRESENTATION_ADDRESS,
			    attr->value.string)) {

				attr2 = attr_list[attr_num].value.object.object;
				while ((attr2->type != DS_ATTRIBUTE_VALUES) &&
				    (attr2->type != OM_NO_MORE_TYPES))
					attr2++;

				if (attr2->type == DS_ATTRIBUTE_VALUES) {

					unsigned char	*pa;
					int		len;

					if (! (pa = xds_paddr_to_string(
					    attr2->value.object.object, len))) {
						delete id;
						delete ref;
						err =
						    FN_E_INSUFFICIENT_RESOURCES;
						return (0);
					}
					ref->append_addr(FN_ref_addr(paddr, len,
					    pa));
					delete [] pa;
				}
			}
		}
	}

	x500_trace("XDSXOM::xds_attr_list_to_ref: created %s (%s) to %s\n",
	    "XFN reference", (rid ? rid->str() : id->str()), name.str());

	delete id;
	return (ref);
}


/*
 * Convert an XDS search result object to an XFN search set or
 * extended search set.
 */
int
XDSDUA::xds_result_to_set(
	const FN_string		&name,
	OM_private_object	result,
	OM_private_object	context,
	unsigned int		return_ref,
	FN_searchset		*set,
	FN_ext_searchset	*ext_set,
	int			&err
)
{
	OM_type			route[] = {DS_SEARCH_INFO, 0};
	OM_type			type[] = {DS_ENTRIES, 0};
	OM_public_object	pub_result = 0;
	OM_value_position	total;

	if (deep_om_get(route, result, OM_EXCLUDE_ALL_BUT_THESE_TYPES, type,
	    OM_FALSE, 0, 0, &pub_result, &total) != OM_SUCCESS) {

		err = FN_E_INSUFFICIENT_RESOURCES;
		om_delete(result);
		return (0);
	}
	x500_trace("XDSXOM::xds_result_to_searchset: retrieved %d entr%s\n",
	    total, (total == 1) ? "y" : "ies");

	// build set of entries with attributes and/or references
	FN_string	*rdn_string;
	FN_string	slash((unsigned char *)"/");
	FN_ref		*ref = 0;
	OM_descriptor	*child;
	OM_descriptor	*entry;
	OM_descriptor	*entry_dn;
	OM_descriptor	*entry_rdn;
	unsigned char	*rdn;

	while (total--) {
		entry = pub_result[total].value.object.object;

		// build name

		// locate DN
		entry_dn = entry;
		entry_dn++;	// skip OM_CLASS
		while (entry_dn->type != DS_OBJECT_NAME)
			entry_dn++;

		// locate final RDN
		child = entry_rdn = entry_dn->value.object.object;
		entry_rdn++;	// skip OM_CLASS
		while (entry_rdn->type == DS_RDNS)
			entry_rdn++;
		entry_rdn--;	// back-up

		XDSDN	xdn(entry_rdn->value.object.object);

		if (! (rdn = xdn.str())) {
			err = FN_E_INSUFFICIENT_RESOURCES;
			om_delete(pub_result);
			return (0);
		}
		if (! (rdn_string = new FN_string(rdn))) {
			err = FN_E_INSUFFICIENT_RESOURCES;
			delete [] rdn;
			om_delete(pub_result);
			return (0);
		}

		// build set of attributes
		FN_attrset	*attrset = 0;
		unsigned int	attr_num;

		attrset = convert_xds_attr_list(entry, 0);
		attr_num = attrset ? attrset->count() : 0;

		x500_trace("XDSXOM::xds_result_to_searchset: retrieved %d %s\n",
		    attr_num, (attr_num == 1) ? "attribute" : "attributes");

		// build reference
		if (return_ref) {

			OM_descriptor	*attrs;
			unsigned int	status;
			FN_string	subordinate(&status, &name, &slash,
					    rdn_string, 0);

			attrs = entry;
			while ((attrs->type != DS_ATTRIBUTES) &&
			    (attrs->type != OM_NO_MORE_TYPES))
				attrs++;

			if ((! attr_num) ||
			    (! (ref = xds_attr_list_to_ref(subordinate, attrs,
			    attr_num, context, child, err)))) {
				delete [] rdn;
				delete attrset;
				om_delete(pub_result);
			}
		}
		if (set) {
			set->add(rdn, ref ? ref : 0, attrset ? attrset : 0, 0);
		} else {
			ext_set->add(rdn, ref ? ref : 0, attrset ? attrset : 0,
			    0);
		}

		delete [] rdn;
		delete attrset;
		if (ref) {
			delete ref;
			ref = 0;
		}
	}
	om_delete(pub_result);
	err = FN_SUCCESS;

	return (FN_SUCCESS);
}


/*
 * Return a handle to the specified symbol in 'LIBXOMXDS_SO'
 */
void *
XDSDUA::get_xomxds_sym(
	const char	*func_name
) const
{
	static void	*xomxds_handle = 0;
	void		*func_handle = 0;

	if (xomxds_handle == 0) {
		if ((xomxds_handle = dlopen(LIBXOMXDS_SO, RTLD_LAZY)) == 0) {

			x500_trace("XDSDUA::get_xomxds_sym: %s\n", dlerror());

			return (0);
		}
	}
	if ((func_handle = dlsym(xomxds_handle, func_name)) != 0) {
		return (func_handle);
	} else {

			x500_trace("XDSDUA::get_xomxds_sym: %s\n", dlerror());

			return (0);
	}
}


/*
 * Dump the supplied OM object on stderr
 */
void
XDSDUA::x500_dump(
#ifdef DEBUG
	OM_object	object
#else
	OM_object
#endif
) const
{
#ifdef DEBUG
	void	*func;

	if (func = get_xomxds_sym("OMPexamin"))
		(*((void *(*)(OM_descriptor_struct *, int, int))func))
		    (object, 0, 2);
#endif
}
