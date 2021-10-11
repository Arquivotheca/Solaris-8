/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)XDSXOM.cc	1.1	96/03/31 SMI"


#include <string.h>
#include <stdio.h>	// sprintf()
#include <stdlib.h>	// bsearch()
#include <ctype.h>	// isalnum()
#include "XDSXOM.hh"
#include "XDSExtern.hh"
#include "XDSDN.hh"


/*
 * XDS/XOM data structure manipulation
 */


// initialize static data
OM_private_object	XDSXOM::auth_context = 0;
OM_boolean		XDSXOM::xfn_pkg = 0;

// string presentation address (RFC-1278 encoding)
const int		XDSXOM::max_paddr_length = 1024;

// string network-address (RFC-1278 encoding)
const int		XDSXOM::max_naddr_length = 128;

// string postal address (RFC-1488 encoding)
const int		XDSXOM::max_post_length = 256;


/*
 * Extract the error code from an XDS error object
 */
int
XDSXOM::xds_error_to_int(
	OM_private_object	&status
) const
{
	OM_private_object	err = status;
	OM_boolean		instance;
	OM_type			tl[] = {DS_PROBLEM, 0};
	OM_public_object	problem;
	OM_value_position	total;

	if (om_instance(status, DS_C_ATTRIBUTE_ERROR, &instance) != OM_SUCCESS)
		return (DS_E_MISCELLANEOUS);

	if (instance == OM_TRUE) {	// only examine the first attr problem

		x500_trace("XDSXOM::xds_error_to_int: %s\n",
		    "DS_C_ATTRIBUTE_ERROR encountered");

		OM_type	tl[] = {DS_PROBLEMS, 0};

		if ((om_get(status,
		    OM_EXCLUDE_ALL_BUT_THESE_TYPES + OM_EXCLUDE_SUBOBJECTS, tl,
		    OM_FALSE, 0, 0, &err, &total) != OM_SUCCESS) && (! total))
			return (DS_E_MISCELLANEOUS);

		err = err->value.object.object;
	}

	if (om_get(err, OM_EXCLUDE_ALL_BUT_THESE_TYPES, tl, OM_FALSE, 0, 0,
	    &problem, &total) != OM_SUCCESS) {

		if (instance == OM_TRUE)
			om_delete(err);
		return (DS_E_MISCELLANEOUS);
	}

	x500_trace("XDSXOM::xds_error_to_int: %d (%s)\n",
	    problem->value.enumeration,
	    xds_problem_to_string((int)problem->value.enumeration));

	if (instance == OM_TRUE)
		om_delete(err);
	om_delete(problem);

	return (total ? problem->value.enumeration : DS_E_MISCELLANEOUS);
}


/*
 * Map XDS error code to XFN error code
 */
int
XDSXOM::xds_error_to_xfn(
	OM_private_object	&status
) const
{
	switch (xds_error_to_int(status)) {
	case DS_E_NO_SUCH_OBJECT:
		return (FN_E_NAME_NOT_FOUND);

	case DS_E_ENTRY_ALREADY_EXISTS:
		return (FN_E_NAME_IN_USE);

	case DS_E_NO_SUCH_ATTRIBUTE_OR_VALUE:
		return (FN_E_NO_SUCH_ATTRIBUTE);

	case DS_E_INVALID_ATTRIBUTE_SYNTAX:
		return (FN_E_INVALID_ATTR_VALUE);

	case DS_E_UNDEFINED_ATTRIBUTE_TYPE:
		return (FN_E_INVALID_ATTR_IDENTIFIER);

	case DS_E_INSUFFICIENT_ACCESS_RIGHTS:
		return (FN_E_CTX_NO_PERMISSION);

	case DS_E_INVALID_CREDENTIALS:
	case DS_E_NO_INFORMATION:
		return (FN_E_AUTHENTICATION_FAILURE);

	case DS_E_BAD_NAME:
	case DS_E_NAMING_VIOLATION:
		return (FN_E_ILLEGAL_NAME);

	case DS_E_UNAVAILABLE:
	case DS_E_BUSY:
		return (FN_E_CTX_UNAVAILABLE);

	case DS_E_COMMUNICATIONS_PROBLEM:
		return (FN_E_COMMUNICATION_FAILURE);

	case DS_E_UNWILLING_TO_PERFORM:
		return (FN_E_OPERATION_NOT_SUPPORTED);

	// %%% more error codes?

	default:
		return (FN_E_UNSPECIFIED_ERROR);
	}
}


/*
 * convert an XFN identifier to an object identifier in ASN.1 BER format.
 */
int
XDSXOM::id_to_om_oid(
	const FN_identifier	&id,
	OM_object_identifier	&oid,
	OM_syntax		*syntax,	// if non-zero, return syntax
	OM_object_identifier	**class_oid	// if non-zero, return class
) const
{
	OM_object_identifier	*oidp;

	oid.length = 0;
	oid.elements = 0;
	switch (id.format()) {
	case FN_ID_ISO_OID_STRING:

		x500_trace("XDSXOM::id_to_om_oid: %s\n",
		    "FN_ID_ISO_OID_STRING identifier encountered");

		if (oidp = string_oid_to_om_oid((const char *)id.str())) {
			oid = *oidp;
			delete oidp;	// delete contents later
		} else {

			x500_trace("XDSXOM::id_to_om_oid: %s\n",
			    "cannot map string to an object identifier");

			return (0);
		}
		break;

	case FN_ID_STRING:

		x500_trace("XDSXOM::id_to_om_oid: %s\n",
		    "FN_ID_STRING identifier encountered");

		if (oidp = string_to_om_oid((unsigned char *)id.str())) {
			oid = *oidp;
			delete oidp;	// delete contents later
		} else {

			x500_trace("XDSXOM::id_to_om_oid: %s\n",
			    "cannot map string to an object identifier");

			return (0);
		}
		break;

	case FN_ID_DCE_UUID:

		x500_trace("XDSXOM::id_to_om_oid: %s\n",
		    "FN_ID_DCE_UUID identifier encountered");

		return (0);

	default:

		x500_trace("XDSXOM::id_to_om_oid: %s\n",
		    "unknown identifier format encountered");

		return (0);
	}

	if (syntax) {
		if (! om_oid_to_syntax(&oid, syntax, class_oid)) {
			*syntax = OM_S_TELETEX_STRING;	// default string syntax

			x500_trace("XDSXOM::id_to_om_oid: %s - %s\n",
			    "unrecognised attribute",
			    "using teletex-string syntax");
		}
	}

	return (1);
}


/*
 * insert an attribute identifier into XDS selection object
 */
OM_private_object
XDSXOM::id_to_xds_selection(
	const FN_identifier	&id
) const
{
	OM_private_object	pri_sel = 0;
	OM_string		oid;
	OM_descriptor		one_attr_type[2];
	static OM_descriptor	select_one_attr[] = {
		{DS_ALL_ATTRIBUTES, OM_S_BOOLEAN, {{OM_FALSE, NULL}}},
		{DS_INFO_TYPE, OM_S_ENUMERATION, {{DS_TYPES_AND_VALUES, NULL}}},
		OM_NULL_DESCRIPTOR
	};


	if (! id_to_om_oid(id, oid, 0, 0)) {
		if (oid.elements)
			delete [] oid.elements;
		return (0);
	}

	fill_om_desc(one_attr_type[0], DS_ATTRIBUTES_SELECTED, oid);

	// add a NULL descriptor
	fill_om_desc(one_attr_type[1]);

	// convert to private object
	if (om_create(DS_C_ENTRY_INFO_SELECTION, OM_FALSE, workspace,
	    &pri_sel) == OM_SUCCESS) {
		om_put(pri_sel, OM_REPLACE_ALL, select_one_attr, 0, 0, 0);
		// %%% om_put() is broken (must use OM_INSERT_AT_END)
		om_put(pri_sel, OM_INSERT_AT_END, one_attr_type, 0, 0, 0);
	}

	delete [] oid.elements;
	return (pri_sel);
}


/*
 * insert one or more attribute identifiers into XDS selection object
 */
OM_private_object
XDSXOM::ids_to_xds_selection(
	const FN_attrset	*ids,
	int			add_obj_class,
	int			&ok
) const
{
	// %%% remove when XDS supports DS_SELECT_ALL_TYPES_AND_VALUES
	static OM_descriptor	select_all[] = {
		OM_OID_DESC(OM_CLASS, DS_C_ENTRY_INFO_SELECTION),
		{DS_ALL_ATTRIBUTES, OM_S_BOOLEAN, {{OM_TRUE, NULL}}},
		{DS_INFO_TYPE, OM_S_ENUMERATION, {{DS_TYPES_AND_VALUES, NULL}}},
		OM_NULL_DESCRIPTOR
	};
	// %%% remove when XDS supports DS_SELECT_NO_ATTRIBUTES
	static OM_descriptor	select_none[] = {
		OM_OID_DESC(OM_CLASS, DS_C_ENTRY_INFO_SELECTION),
		{DS_ALL_ATTRIBUTES, OM_S_BOOLEAN, {{OM_FALSE, NULL}}},
		{DS_INFO_TYPE, OM_S_ENUMERATION, {{DS_TYPES_AND_VALUES, NULL}}},
		OM_NULL_DESCRIPTOR
	};
	static OM_descriptor	select_obj_class[] = {
		OM_OID_DESC(OM_CLASS, DS_C_ENTRY_INFO_SELECTION),
		{DS_ALL_ATTRIBUTES, OM_S_BOOLEAN, {{OM_FALSE, NULL}}},
		OM_OID_DESC(DS_ATTRIBUTES_SELECTED, DS_A_OBJECT_CLASS),
		{DS_INFO_TYPE, OM_S_ENUMERATION, {{DS_TYPES_AND_VALUES, NULL}}},
		OM_NULL_DESCRIPTOR
	};
	static OM_descriptor	select_attrs[] = {
		OM_OID_DESC(OM_CLASS, DS_C_ENTRY_INFO_SELECTION),
		{DS_ALL_ATTRIBUTES, OM_S_BOOLEAN, {{OM_FALSE, NULL}}},
		{DS_INFO_TYPE, OM_S_ENUMERATION, {{DS_TYPES_AND_VALUES, NULL}}},
		OM_NULL_DESCRIPTOR
	};

	if (! ids)
		return (select_all);

	int	id_num = ids->count();

	if (! id_num) {
		if (add_obj_class)
			return (select_obj_class);
		else
			return (select_none);
	}

	OM_private_object	pri_sel = 0;
	OM_descriptor		*attr_types;
	OM_descriptor		*attr_type;
	OM_string		oid;
	const FN_attribute	*at;
	const FN_identifier	*id;
	void			*iter;
	int			i;

	attr_types = new OM_descriptor [id_num + 1];
	if (! attr_types) {
		ok = 0;
		return (0);
	}
	attr_type = attr_types;

	for (at = ids->first(iter);
	    (at && (id_num > 0));
	    at = ids->next(iter), attr_type++, id_num--) {

		id = at->identifier();
		if ((! id) || (! id_to_om_oid(*id, oid, 0, 0))) {

			// delete oid(s)
			for (i = 0; i < id_num; i++) {
				delete [] attr_types[i].value.string.elements;
			}
			delete [] attr_types;
			ok = 0;
			return (0);
		}

		fill_om_desc(*attr_type, DS_ATTRIBUTES_SELECTED, oid);
	}
	// add a NULL descriptor
	fill_om_desc(*attr_type);

	// convert to private object
	if (om_create(DS_C_ENTRY_INFO_SELECTION, OM_FALSE, workspace,
	    &pri_sel) == OM_SUCCESS) {
		if (add_obj_class) {
			om_put(pri_sel, OM_REPLACE_ALL, select_obj_class, 0, 0,
			    0);
		} else {
			om_put(pri_sel, OM_REPLACE_ALL, select_attrs, 0, 0, 0);
		}
		// %%% om_put() is broken (must use OM_INSERT_AT_END)
		om_put(pri_sel, OM_INSERT_AT_END, attr_types, 0, 0, 0);
	}

	// delete oid(s)
	for (i = 0; i < id_num; i++) {
		delete [] attr_types[i].value.string.elements;
	}
	delete [] attr_types;
	return (pri_sel);
}


/*
 * Map an XDS object identifier onto an XFN reference type
 *
 * DS_O_ORG               -> onc_fn_enterprise
 * DS_O_ORG_UNIT          -> onc_fn_organization
 * DS_O_PERSON            -> onc_fn_user
 * DS_O_LOCALITY          -> onc_fn_site
 * DS_O_APPLIC_PROCESS    -> onc_fn_service
 *
 * all others are mapped to the reference type: x500
 */

FN_identifier *
XDSXOM::xds_attr_to_ref_type(
	OM_public_object	attr
) const
{
	OM_public_object	a = attr;

	while (a->type != DS_ATTRIBUTE_VALUES &&
	    a->type != OM_NO_MORE_TYPES)
		a++;

	while (a->type == DS_ATTRIBUTE_VALUES) {

		if (compare_om_oids(DS_O_TOP, a->value.string)) {
			a++;
			continue;
		}

		if (compare_om_oids(DS_O_ALIAS, a->value.string) ||
		    compare_om_oids(DS_O_GROUP_OF_NAMES, a->value.string) ||
		    compare_om_oids(DS_O_COUNTRY, a->value.string))
			break;

		// MHS object classes
		if (memcmp("\x55\x05\x01", a->value.string.elements, 3) == 0)
			break;

		if (compare_om_oids(DS_O_PERSON, a->value.string))
			return (new FN_identifier(
			    (const unsigned char *)"onc_fn_user"));

		if (compare_om_oids(DS_O_ORG, a->value.string))
			return (new FN_identifier(
			    (const unsigned char *)"onc_fn_enterprise"));

		if (compare_om_oids(DS_O_ORG_UNIT, a->value.string))
			return (new FN_identifier(
			    (const unsigned char *)"onc_fn_organization"));

		if (compare_om_oids(DS_O_LOCALITY, a->value.string))
			return (new FN_identifier(
			    (const unsigned char *)"onc_fn_site"));

		if (compare_om_oids(DS_O_APPLIC_PROCESS, a->value.string))
			return (new FN_identifier(
			    (const unsigned char *)"onc_fn_service"));

		a++;
	}

	return (new FN_identifier((const unsigned char *)"x500"));
}


/*
 * Convert an XFN reference into an octet string (DS_A_NNS_REF_STRING or
 * DS_A_OBJECT_REF_STRING). Fill the syntax and value components of the
 * supplied OM_descriptor.
 */
int
XDSXOM::ref_to_xds_attr_value(
	const FN_ref	*ref,
	OM_object	attr_value
) const
{
	unsigned char	*cp;
	int		len;

	if (! ref)
		return (0);

	if (xfn_pkg) {

		// %%% build DS_C_REF_IDENT, DS_C_REF_ADDRESSES from reference
		attr_value->syntax = OM_S_OBJECT;

		x500_trace("XDSXOM::ref_to_xds_attr_value: %s\n",
		    "XOM object classes for reference not supported");

		return (0);

	} else {	// workaround

		x500_trace("XDSXOM::ref_to_xds_attr_value: %s\n",
		    "string-encoded reference supported");

		// build octet string from ref

		if (! (cp = ref_to_string_ref(ref, &len))) {
			return (0);
		}
		attr_value->syntax = OM_S_OCTET_STRING;
		attr_value->value.string.length = len;
		attr_value->value.string.elements = cp;
	}

	return (1);
}


/*
 * Convert an octet string (DS_A_NNS_REF_STRING or DS_A_OBJECT_REF_STRING)
 * into an XFN reference.
 */
FN_ref *
XDSXOM::xds_attr_value_to_ref(
	OM_object	attr_value,
	FN_attrset	**attrs,
	int		&ref_err
) const
{
	FN_ref	*ref;

	if (xfn_pkg) {

		// %%% build reference from DS_C_REF_IDENT, DS_C_REF_ADDRESSES
		if (attr_value->syntax != OM_S_OBJECT) {
			ref_err = FN_E_OPERATION_NOT_SUPPORTED;
			return (0);
		}

		x500_trace("XDSXOM::xds_attr_value_to_ref: %s\n",
		    "XOM object classes for reference not supported");

		ref_err = FN_E_OPERATION_NOT_SUPPORTED;
		return (0);

	} else {	// workaround

		x500_trace("XDSXOM::xds_attr_value_to_ref: %s\n",
		    "string-encoded reference supported");

		// build reference from octet string
		if ((attr_value->syntax != OM_S_OCTET_STRING) ||
		    (! (ref = string_ref_to_ref(
		    (unsigned char *)attr_value->value.string.elements,
		    (int)attr_value->value.string.length, attrs, ref_err)))) {

			ref_err = FN_E_MALFORMED_REFERENCE;
			return (0);
		}
	}
	return (ref);
}


/*
 * Convert a string into an XDS attribute value
 */
int
XDSXOM::string_to_xds_attr_value(
	FN_string		*val_str,
	const OM_syntax		syntax,
	OM_object_identifier	*class_oid,
	OM_descriptor		*attr_value
) const
{
	if (! val_str)
		return (0);

	switch (syntax) {

	case OM_S_PRINTABLE_STRING:
	case OM_S_TELETEX_STRING:
	case OM_S_IA5_STRING:
	case OM_S_NUMERIC_STRING:
	case OM_S_VISIBLE_STRING:
		attr_value->value.string.length = val_str->bytecount();
		attr_value->value.string.elements = (void *)val_str->contents();
		return (1);

	case OM_S_OCTET_STRING:
		attr_value->value.string.length = val_str->bytecount();
		attr_value->value.string.elements = (void *)val_str->contents();
		return (1);

	case OM_S_OBJECT_IDENTIFIER_STRING: {
		OM_object_identifier	*oid;

		if (oid = string_to_om_oid((unsigned char *)val_str->str())) {
			attr_value->value.string.length = oid->length;
			attr_value->value.string.elements = oid->elements;
			delete oid;	// delete contents later
			return (1);
		} else if (oid = string_oid_to_om_oid(
		    (const char *)val_str->str())) {
			attr_value->value.string.length = oid->length;
			attr_value->value.string.elements = oid->elements;
			delete oid;	// delete contents later
			return (1);
		} else {

			x500_trace("XDSXOM::string_to_xds_attr_value: %s\n",
			    "cannot map string to an object identifier");

			return (0);
		}
	}

	case OM_S_ENUMERATION:
	case OM_S_INTEGER:
	case OM_S_BOOLEAN:
		attr_value->value.enumeration = atoi((char *)val_str->str());
		return (1);

	case OM_S_OBJECT:

		x500_trace("XDSXOM::string_to_xds_attr_value: %s\n",
		    "non-string syntax encountered");

		if (compare_om_oids(DS_C_DS_DN, *class_oid)) {

			XDSDN	*xdn;

			if (! (xdn = new XDSDN(*val_str))) {
				return (0);
			} else {
				attr_value->value.object.object =
				    xdn->internal();
			}

		} else if (compare_om_oids(DS_C_PRESENTATION_ADDRESS,
		    *class_oid)) {

			if (! (attr_value->value.object.object =
				string_to_xds_paddr(*val_str))) {
				return (0);
			}

		} else if (compare_om_oids(DS_C_POSTAL_ADDRESS, *class_oid)) {

			if (! (attr_value->value.object.object =
				string_to_xds_post_addr(*val_str))) {
				return (0);
			}

		} else {
			// %%% handle other non-string syntaxes

			return (0);
		}

		return (1);

	default:
		return (0);
	}
}


/*
 * Release resources associated with an attribute value
 */
void
XDSXOM::delete_xds_attr_value(
	OM_descriptor	*attr_value
) const
{
	switch (attr_value->syntax & OM_S_SYNTAX) {

	case OM_S_PRINTABLE_STRING:
	case OM_S_TELETEX_STRING:
	case OM_S_OCTET_STRING:
	case OM_S_IA5_STRING:
	case OM_S_NUMERIC_STRING:
	case OM_S_VISIBLE_STRING:
		break;	// do nothing - supplied string was not copied

	case OM_S_OBJECT_IDENTIFIER_STRING:
		delete [] attr_value->value.string.elements;
		break;


	case OM_S_ENUMERATION:
	case OM_S_INTEGER:
	case OM_S_BOOLEAN:
		break;

	case OM_S_OBJECT: {
		OM_object		obj = attr_value->value.object.object;

		if (attr_value->syntax & OM_S_PRIVATE) {
			om_delete(obj);
			break;
		}
		OM_object_identifier	*class_oid = &obj->value.string;
		OM_descriptor		*dp = obj;

		if (compare_om_oids(DS_C_DS_DN, *class_oid)) {

			break;	// always private

		} else if (compare_om_oids(DS_C_PRESENTATION_ADDRESS,
		    *class_oid)) {

			dp++; 	// skip OM_CLASS
			while (dp->type != OM_NO_MORE_TYPES) {
				delete [] dp->value.string.elements;
				dp++;
			}

		} else if (compare_om_oids(DS_C_POSTAL_ADDRESS, *class_oid)) {

			dp++; 	// skip OM_CLASS
			while (dp->type != OM_NO_MORE_TYPES) {
				delete [] dp->value.string.elements;
				dp++;
			}
		}
		// %%% handle other non-string syntaxes

		break;
	}

	default:
		break;
	}
}


/*
 * Convert an XDS attribute value into OM string format
 */
unsigned char *
XDSXOM::xds_attr_value_to_string(
	OM_descriptor	*attr_value,
	unsigned int	&format,
	unsigned int	&length
) const
{
	OM_string	*val = &attr_value->value.string;
	unsigned char	*str;

	switch (attr_value->syntax & OM_S_SYNTAX) {

	case OM_S_PRINTABLE_STRING:
	case OM_S_TELETEX_STRING:
	case OM_S_IA5_STRING:
	case OM_S_NUMERIC_STRING:
	case OM_S_VISIBLE_STRING:
		if (! (str = new unsigned char [val->length + 1]))
			return (0);

		memcpy(str, val->elements, (size_t)val->length);
		str[val->length] = '\0';
		length = (unsigned int) val->length + 1; // %%% add 1, for now
		format = FN_ID_STRING;
		return (str);

	case OM_S_OBJECT_IDENTIFIER_STRING:
		if (str = om_oid_to_string(val)) {
			format = FN_ID_STRING;
		} else {
			str = om_oid_to_string_oid(val);
			format = FN_ID_ISO_OID_STRING;
		}
		length = strlen((char *)str);
		return (str);

	case OM_S_OBJECT: {
		x500_trace("XDSXOM::xds_attr_value_to_string: %s\n",
		    "non-string syntax encountered");

		OM_object		value = attr_value->value.object.object;
		OM_object_identifier	*class_oid = &value->value.string;
		int			len;

		if (compare_om_oids(DS_C_DS_DN, *class_oid)) {

			XDSDN	xdn(value);

			if (! (str = xdn.str(1))) {
				return (0);
			}

		} else if (compare_om_oids(DS_C_PRESENTATION_ADDRESS,
		    *class_oid)) {

			if (! (str = xds_paddr_to_string(value, len))) {
				return (0);
			}

		} else if (compare_om_oids(DS_C_POSTAL_ADDRESS, *class_oid)) {

			if (! (str = xds_post_addr_to_string(value, len))) {
				return (0);
			}

		} else {
			// %%% handle other non-string syntaxes

			return (0);
		}
		length = strlen((char *)str);
		format = FN_ID_STRING;
		return (str);
	}

	case OM_S_OCTET_STRING:

		x500_trace("XDSXOM::xds_attr_value_to_string: %s\n",
		    "octet-string syntax encountered");

		if (! (str = new unsigned char [val->length + 1]))
			return (0);

		memcpy(str, val->elements, (size_t)val->length);
		length = (unsigned int) val->length;
		format = FN_ID_STRING + 99;	// must not be FN_ID_STRING
		return (str);

	case OM_S_ENUMERATION:
	case OM_S_INTEGER:
	case OM_S_BOOLEAN: {

		unsigned char	*num = new unsigned char[32];

		sprintf((char *)num, "%d", attr_value->value.enumeration);
		length = strlen((char *)num);
		format = FN_ID_STRING;
		return (num);
	}

	default:
		return (0);
	}
}


/*
 * Convert a presentation address (in the string format specified in
 * RFC-1278) into XDS format (class DS_C_PRESENTATION_ADDRESS).
 *
 * [[[ <p-sel> '/' ] <s-sel> '/' ] <t-sel> '/' ] { <n-addr> }+
 *
 * where: [ x ] denotes optional x
 *        { x }+ denotes one or more occurrences of x
 *        <p-sel> is a presentation selector in hex format
 *        <s-sel> is a session selector in hex format
 *        <t-sel> is a transport selector in hex format
 *        <n-addr> is a network address in hex format. Each address has a
 *                 prefix of 'NS+' and multiple addresses are is linked by
 *                 an '_'.
 *
 * e.g. "ses"/NS+b1b2b3  or  'a1a2a3'H/NS+b1b2b3_NS+c1c2c3"
 */
OM_public_object
XDSXOM::string_to_xds_paddr(
	const FN_string	&paddress
) const
{
	unsigned char	*paddr_string = (unsigned char *)paddress.str();
	unsigned char	*cp = paddr_string;
	unsigned char	*cp1;
	unsigned char	*cp2;
	unsigned char	*cp3;
	unsigned char	*cp4;
	int		sel_num = 0;
	int		na_num = 0;

	cp1 = cp2 = cp3 = 0;
	while (*cp) {
		if (*cp == '/')
			sel_num++;
		else if (*cp == '+')
			na_num++;
		cp++;
	}

	x500_trace("XDSXOM::string_to_xds_paddr: %d sel's, %d n-addr's\n",
	    sel_num, na_num);

	if ((na_num == 0) || (sel_num > 3)) {
		return (0);
	}

	OM_descriptor	*pa = new OM_descriptor [sel_num + na_num + 2];
	int		ipa = 0;	// index
	unsigned char	*buf = new unsigned char [max_naddr_length];
					// tmp buffer for selector or n-address
	OM_string	*sel;

	if ((! pa) || (! buf)) {
		if (pa)
			delete [] pa;
		if (buf)
			delete [] buf;

		return (0);
	}
	cp = paddr_string;	// reset

	while (*cp) {

		// build a DS_C_PRESENTATION_ADDRESS object

		fill_om_desc(pa[ipa++], OM_CLASS, DS_C_PRESENTATION_ADDRESS);

		switch (sel_num) {

		case 3:
			fill_om_desc(pa[ipa], DS_P_SELECTOR,
			    OM_S_OCTET_STRING, (void *)0, 0);
			sel = &pa[ipa++].value.string;
			sel->elements = buf;

			if (! (cp = string_to_xds_paddr_selector(cp, sel))) {
				delete [] pa;
				delete [] buf;
				return (0);
			}

			if (! (cp3 = new unsigned char [sel->length])) {
				delete [] pa;
				delete [] buf;
				return (0);
			} else {
				sel->elements = memcpy(cp3, sel->elements,
				    (size_t)sel->length);
			}

			// FALL-THROUGH

		case 2:
			fill_om_desc(pa[ipa], DS_S_SELECTOR,
			    OM_S_OCTET_STRING, (void *)0, 0);
			sel = &pa[ipa++].value.string;
			sel->elements = buf;

			if (! (cp = string_to_xds_paddr_selector(cp, sel))) {
				delete [] cp3;
				delete [] pa;
				delete [] buf;
				return (0);
			}

			if (! (cp2 = new unsigned char [sel->length])) {
				delete [] cp3;
				delete [] pa;
				delete [] buf;
				return (0);
			} else {
				sel->elements = memcpy(cp2, sel->elements,
				    (size_t)sel->length);
			}

			// FALL-THROUGH

		case 1:
			fill_om_desc(pa[ipa], DS_T_SELECTOR,
			    OM_S_OCTET_STRING, (void *)0, 0);
			sel = &pa[ipa++].value.string;
			sel->elements = buf;

			if (! (cp = string_to_xds_paddr_selector(cp, sel))) {
				delete [] cp2;
				delete [] cp3;
				delete [] pa;
				delete [] buf;
				return (0);
			}

			if (! (cp1 = new unsigned char [sel->length])) {
				delete [] cp2;
				delete [] cp3;
				delete [] pa;
				delete [] buf;
				return (0);
			} else {
				sel->elements = memcpy(cp1, sel->elements,
				    (size_t)sel->length);
			}
			break;

		default:
			delete [] pa;
			delete [] buf;
			return (0);
		}

		int		i;
		unsigned char	*bp;

		for (i = 0; i < na_num; i++) {

			bp = buf;	// reset

			fill_om_desc(pa[ipa], DS_N_ADDRESSES,
			    OM_S_OCTET_STRING, (void *)0, 0);
			sel = &pa[ipa++].value.string;
			sel->elements = buf;

			if ((cp[0] == 'N') && (cp[1] == 'S') && (cp[2] == '+'))
				cp += 3;	// skip "NS+" prefix
			else {
				delete [] cp1;
				delete [] cp2;
				delete [] cp3;
				delete [] pa;
				delete [] buf;
				return (0);
			}

			unsigned char	hex_string[3];

			hex_string[2] = '\0';
			while (*cp && (*cp != '_')) {

				hex_string[0] = *cp++;
				if (*cp) {
					hex_string[1] = *cp++;
				} else {
					delete [] cp1;
					delete [] cp2;
					delete [] cp3;
					delete [] pa;
					delete [] buf;
					return (0);
				}
				*bp++ = (unsigned char)strtol(
				    (char *)hex_string, (char **)0, 16);
			}
			if (i > 0) {
				if (*cp++ != '_') {
					delete [] cp1;
					delete [] cp2;
					delete [] cp3;
					delete [] pa;
					delete [] buf;
					return (0);	// no separator
				}
			}
			sel->length = bp - buf;
			if (! (cp4 = new unsigned char [sel->length])) {
				delete [] cp1;
				delete [] cp2;
				delete [] cp3;
				delete [] pa;
				delete [] buf;
				return (0);
			}
			sel->elements = memcpy(cp4, sel->elements,
			    (size_t)sel->length);
		}
	}
	fill_om_desc(pa[ipa]);
	delete [] buf;

	return (pa);
}


/*
 * Convert a presentation address selector into its XDS encoding
 *
 * (e.g. encoding 'a1a2a3'H produces "\xa1\xa2\xa3"
 *       encoding "DSA"     produces "\x44\x53\x41")
 */
unsigned char *
XDSXOM::string_to_xds_paddr_selector(
	unsigned char	*string,
	OM_string	*selector	// supplied buffer
) const
{
	unsigned char	*cp = string;
	unsigned char	*bp = (unsigned char *)selector->elements;

	if (*cp++ == '"') {	// IA5

		while (*cp && (*cp != '"'))
			*bp++ = *cp++;

		if (*cp == '"')
			cp++;
		else
			return (0);	// no closing quote

		if (*cp++ != '/')
			return (0);	// no terminating slash

	} else if (*cp++ == '\'') {

		unsigned char	hex_string[3];

		hex_string[2] = '\0';
		while (*cp && (*cp != '\'')) {

			hex_string[0] = *cp++;
			if (*cp) {
				hex_string[1] = *cp++;
			} else {
				return (0);
			}
			*bp++ = (unsigned char)strtol((char *)hex_string,
			    (char **)NULL, 16);
		}

		if (*cp++ != '\'')
			return (0);	// no closing quote

		if (*cp++ != 'H')
			return (0);

		if (*cp++ != '/')
			return (0);	// no terminating slash

	} else
		return (0);

	selector->length = bp - (unsigned char *)selector->elements;
	return (cp);
}


/*
 * Convert a presentation address (in XDS format) into the string format
 * specified in RFC-1278 and set its length.
 *
 * [[[ <p-sel> '/' ] <s-sel> '/' ] <t-sel> '/' ] { <n-addr> }+
 *
 * where: [ x ] denotes optional x
 *        { x }+ denotes one or more occurrences of x
 *        <p-sel> is a presentation selector in hex format
 *        <s-sel> is a session selector in hex format
 *        <t-sel> is a transport selector in hex format
 *        <n-addr> is a network address in hex format. Each address has a
 *                 prefix of 'NS+' and multiple addresses are is linked by
 *                 an '_'.
 *
 * e.g. "ses"/NS+b1b2b3  or  'a1a2a3'H/NS+b1b2b3_NS+c1c2c3"
 */
unsigned char *
XDSXOM::xds_paddr_to_string(
	OM_object	paddress,
	int		&len
) const
{
	unsigned char		paddr_string[max_paddr_length];
	unsigned char		*cp = paddr_string;
	unsigned char		*cp2;
	OM_public_object	pub_paddr = 0;
	OM_value_position	total;
	OM_object		pa = 0;

	if (! paddress)
		return (0);

	// convert to public
	if (paddress->type == OM_PRIVATE_OBJECT) {
		if (om_get(paddress, OM_NO_EXCLUSIONS, 0, OM_FALSE, 0, 0,
		    &pub_paddr, &total) != OM_SUCCESS) {
			return (0);
		}
		pa = pub_paddr;
	} else
		pa = paddress;

	OM_object	pa2 = pa;

	// locate presentation-selector (if present)
	while ((pa->type != OM_NO_MORE_TYPES) && (pa->type != DS_P_SELECTOR))
		pa++;

	if (pa->type == DS_P_SELECTOR) {
		cp = xds_paddr_selector_to_string(&pa->value.string, cp);
		*cp++ = '/';
	}
	pa = pa2;	// reset

	// locate session-selector (if present)
	while ((pa->type != OM_NO_MORE_TYPES) && (pa->type != DS_S_SELECTOR))
		pa++;

	if (pa->type == DS_S_SELECTOR) {
		cp = xds_paddr_selector_to_string(&pa->value.string, cp);
		*cp++ = '/';
	}
	pa = pa2;	// reset

	// locate transport-selector (if present)
	while ((pa->type != OM_NO_MORE_TYPES) && (pa->type != DS_T_SELECTOR))
		pa++;

	if (pa->type == DS_T_SELECTOR) {
		cp = xds_paddr_selector_to_string(&pa->value.string, cp);
		*cp++ = '/';
	}
	pa = pa2;	// reset

	// locate network-address(es)
	while ((pa->type != OM_NO_MORE_TYPES) && (pa->type != DS_N_ADDRESSES))
		pa++;

	while (pa->type == DS_N_ADDRESSES) {
		int	i;
		int	j = (int)pa->value.string.length;
		unsigned char	*addr =
				    (unsigned char *)pa->value.string.elements;

		*cp++ = 'N';
		*cp++ = 'S';
		*cp++ = '+';

		for (i = 0; i < j; i++) {
			cp += sprintf((char *)cp, "%.2x", *addr++);
		}
		*cp++ = '_';
		pa++;
	}
	*--cp = '\0';	// remove trailing underscore

	len = cp - paddr_string + 1;

	x500_trace("XDSXOM::xds_paddr_to_string: length=%d\n", len);

	if (! (cp2 = new unsigned char [len])) {
		return (0);
	}

	return ((unsigned char *)memcpy(cp2, paddr_string, (size_t)len));
}


/*
 * Convert a presentation address selector into its RFC-1278 string encoding
 *
 * (e.g. decoding "\xa1\xa2\xa3" produces 'a1a2a3'H
 *       decoding "\x44\x53\x41" produces "DSA")
 */
unsigned char *
XDSXOM::xds_paddr_selector_to_string(
	OM_string	*selector,
	unsigned char	*string		// supplied buffer
) const
{
	int		len = (int)selector->length;
	unsigned char	*sel = (unsigned char *)selector->elements;
	unsigned char	*cp = string;
	int	i;

	// assume IA5
	*cp++ = '"';
	for (i = 0; i < len; i++) {

		if ((isalnum(*sel)) || (*sel == '+') || (*sel == '-') ||
		    (*sel == '.')) {

			*cp++ = *sel++;
		} else
			break;	// not IA5
	}
	*cp++ = '"';
	if (i != len) {
		sel = (unsigned char *)selector->elements;	// reset
		cp = string;	// reset

		*cp++ = '\'';
		for (i = 0; i < len; i++) {
			cp += sprintf((char *)cp, "%.2x", *sel++);
		}
		*cp++ = '\'';
		*cp++ = 'H';
	}
	return (cp);
}

/*
 * Convert a postal address (in the string format specified in RFC-1488)
 * into XDS format (DS_C_POSTAL_ADDRESS).
 *
 * <line> [ '$' <line> ]
 *
 * where: [ x ] denotes optional x
 *        <line> is a line of a postal address (may have upto 6 lines)
 *
 * e.g. "2550 Garcia Ave $ Mountain View $ CA 94043-1100"
 */
OM_public_object
XDSXOM::string_to_xds_post_addr(
	const FN_string	&post
) const
{
	unsigned char	*post_string = (unsigned char *)post.str();
	unsigned char	*cp = post_string;
	int		line_num = 1;

	while (*cp) {
		if (*cp == '$')
			line_num++;
		cp++;
	}

	x500_trace("XDSXOM::string_to_xds_post_addr: %d lines\n", line_num);

	if ((line_num > 6) ||
	    (cp - post_string > ((line_num * 30) + line_num))) {
		return (0);
	}

	OM_descriptor	*po = new OM_descriptor [line_num + 2];
	int		ipo = 0;	// index
	int		len = 0;
	OM_string	*line;

	if (! po)
		return (0);

	cp = post_string;	// reset

	while (*cp) {

		// build a DS_C_POSTAL_ADDRESS object

		fill_om_desc(po[ipo++], OM_CLASS, DS_C_POSTAL_ADDRESS);

		while (line_num--) {

			fill_om_desc(po[ipo], DS_POSTAL_ADDRESS,
			    OM_S_TELETEX_STRING, (void *)0, 0);
			line = &po[ipo++].value.string;

			unsigned char	*buf = new unsigned char [32];
			unsigned char	*bp = buf;

			while (*cp && (*cp != '$'))
				*bp++ = *cp++;

			if (*cp == '$')
				cp++;

			line->elements = buf;
			line->length = bp - buf;
		}
		fill_om_desc(po[ipo]);
	}
	return (po);
}


/*
 * Convert a postal address (in XDS format) into the string format
 * specified in RFC-1488 and set its length.
 *
 * <line> [ '$' <line> ]
 *
 * where: [ x ] denotes optional x
 *        <line> is a line of a postal address (may have upto 6 lines)
 *
 * e.g. "2550 Garcia Ave $ Mountain View $ CA 94043-1100"
 */
unsigned char *
XDSXOM::xds_post_addr_to_string(
	OM_object	post,
	int		&len
) const
{
	unsigned char		post_string[max_post_length];
	unsigned char		*cp = post_string;
	unsigned char		*cp2;
	OM_public_object	pub_post = 0;
	OM_value_position	total;
	OM_object		po = 0;

	if (! post)
		return (0);

	// convert to public
	if (post->type == OM_PRIVATE_OBJECT) {
		if (om_get(post, OM_NO_EXCLUSIONS, 0, OM_FALSE, 0, 0,
		    &pub_post, &total) != OM_SUCCESS) {
			return (0);
		}
		po = pub_post;
	} else
		po = post;

	po++;	// skip OM_CLASS
	while (po->type == DS_POSTAL_ADDRESS) {
		cp = (unsigned char *)memcpy(cp, po->value.string.elements,
		    (size_t)po->value.string.length);
		cp += po->value.string.length;
		*cp++ = '$';
		po++;
	}
	*--cp = '\0';	// remove trailing dollar

	len = cp - post_string + 1;

	x500_trace("XDSXOM::xds_post_addr_to_string: length=%d\n", len);

	if (pub_post)
		om_delete(pub_post);

	if (! (cp2 = new unsigned char [len])) {
		return (0);
	}

	return ((unsigned char *)memcpy(cp2, post_string, (size_t)len));
}


/*
 * Convert an XDS attribute list object
 *
 * NOTE: attributes are either converted to an XFN attribute set
 *       or appended to an XFN reference, when it is supplied
 */
FN_attrset *
XDSXOM::convert_xds_attr_list(
	OM_public_object	entry,
	FN_ref			*ref
) const
{
	FN_attrset	*attrs = 0;
	OM_descriptor	*e = entry;
	OM_descriptor	*at;
	OM_descriptor	*at2;
	OM_string 	*val;
	OM_string	str;
	FN_identifier	*id;
	FN_attribute	*attr;
	unsigned int	format;
	unsigned int	length;

	while (e && (e->type != OM_NO_MORE_TYPES)) {

		if (e->type != DS_ATTRIBUTES) {
			e++;
			continue;
		}

		at2 = at = e->value.object.object + 1;
		attr = 0;

		while (at->type != OM_NO_MORE_TYPES) {
			val = &at->value.string;

			if (at->type == DS_ATTRIBUTE_TYPE) {

				if (! (str.elements =
				    xds_attr_value_to_string(at, format,
				    length))) {

					x500_trace("XDSXOM::%s %s %s\n",
					    "convert_xds_attr_list:",
					    "cannot convert attribute",
					    "type to string");

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

					x500_trace("XDSXOM::%s %s %s\n",
					    "convert_xds_attr_list:",
					    "cannot convert attribute",
					    "value to string");

					continue;	// ignore it
				}


				// DO THIS WHEN REFERENCE IS NOT SUPPLIED
				if (! ref) {
					if (! attr) {
						attr = new FN_attribute(*id,
					    ((format == FN_ID_STRING) ||
					    (format == FN_ID_ISO_OID_STRING) ||
					    (format == FN_ID_DCE_UUID)) ?
						    ascii : octet);
					}

					attr->add(FN_attrvalue(str.elements,
					    length));

				} else { // DO THIS WHEN REFERENCE IS SUPPLIED
					FN_ref_addr	addr(*id, length,
							    str.elements);

					ref->append_addr(addr);
				}


				delete [] str.elements;
					// xds_attr_value_to_string() allocates
			}
			at++;
		}


		// DO THIS WHEN REFERENCE IS NOT SUPPLIED
		if (! ref) {
			if (! attrs)
				attrs = new FN_attrset;

			attrs->add(*attr);
			delete attr;
		}


		delete id;
		e++;
	}
	return (attrs);
}


/*
 * Convert an XFN attribute set into an XDS entry modification list object.
 * Append XFN reference if supplied.
 */
OM_private_object
XDSXOM::attrs_to_xds_entry_mod_list(
	const FN_attrset	*attrs,
	const FN_ref		*ref,
	int			&err
) const
{
	int	attr_num;

	if ((! attrs) || ((attr_num = attrs->count()) == 0)) {
		err = FN_E_ATTR_VALUE_REQUIRED;
		return (0);
	}

	x500_trace("XDSXOM::attrs_to_xds_entry_mod_list: %d XFN attribute(s)\n",
	    attr_num);

	OM_private_object	pri_mod_list = 0;
	OM_descriptor		*mod_list;
	OM_descriptor		*ml;
	OM_descriptor		*attribute;
	OM_descriptor		*ref_attr = 0;
	const FN_attribute	*attr;
	const FN_attrvalue	*val;
	const FN_identifier	*id;
	void			*attr_iter;
	void			*val_iter;
	OM_string		oid;
	OM_syntax		syntax;
	FN_string		*val_str;
	int			val_num;
	int			add_ref = 0;
	int			ok = 1;
	int			i;
	int			j;

	if (ref)
		add_ref = 1;

	if ((mod_list = new OM_descriptor [attr_num + 2 + add_ref]) == 0) {
		err = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	fill_om_desc(mod_list[0], OM_CLASS, DS_C_ENTRY_MOD_LIST);
	fill_om_desc(mod_list[1]); // zero it

	for (i = 0, attr = attrs->first(attr_iter);
	    (i < attr_num) && (ok);
	    attr = attrs->next(attr_iter), i++) {

		fill_om_desc(mod_list[i + 1]); // zero it

		val_num = attr->valuecount();
		if ((attribute = new OM_descriptor [val_num + 4]) == 0) {
			err = FN_E_INSUFFICIENT_RESOURCES;
			ok = 0;
			continue;
		}
		fill_om_desc(mod_list[i + 1], DS_CHANGES, attribute);
		fill_om_desc(mod_list[i + 2]); // zero it
		fill_om_desc(attribute[0]); // zero it

		// extract attribute identifier
		id = attr->identifier();
		if ((! id) || (! id_to_om_oid(*id, oid, &syntax, 0))) {
			if (oid.elements)
				delete [] oid.elements;
			err = FN_E_INVALID_ATTR_IDENTIFIER;
			ok = 0;
			continue;
		}
		fill_om_desc(attribute[0], OM_CLASS, DS_C_ENTRY_MOD);
		fill_om_desc(attribute[1], DS_ATTRIBUTE_TYPE, oid);
		fill_om_desc(attribute[val_num + 2], DS_MOD_TYPE,
		    DS_ADD_ATTRIBUTE, OM_S_ENUMERATION);
		fill_om_desc(attribute[val_num + 3]);

		for (j = 0, val = attr->first(val_iter);
		    (j < val_num) && (ok);
		    val = attr->next(val_iter), j++) {

			attribute[j + 2].type = DS_ATTRIBUTE_VALUES;
			attribute[j + 2].syntax = syntax;

			val_str = val->string();
			if (! string_to_xds_attr_value(val_str, syntax, 0,
			    &attribute[j + 2])) {
				delete val_str;
				err = FN_E_INVALID_ATTR_VALUE;
				ok = 0;
				fill_om_desc(attribute[j + 2]); // zero it
				continue;
			}
		}
	}
	if (ok)
		fill_om_desc(mod_list[i + 1]); // zero it

	// convert ref and append to XDS entry modification list
	if (ref && ok) {
		if ((ref_attr = new OM_descriptor [5]) == 0) {
			err = FN_E_INSUFFICIENT_RESOURCES;
			delete [] mod_list;
			return (0);
		}
		fill_om_desc(mod_list[i + 1], DS_CHANGES, ref_attr);
		fill_om_desc(ref_attr[0], OM_CLASS, DS_C_ENTRY_MOD);
		fill_om_desc(ref_attr[1], DS_ATTRIBUTE_TYPE,
		    DS_A_OBJECT_REF_STRING);
		fill_om_desc(ref_attr[2], DS_ATTRIBUTE_VALUES,
		    (OM_descriptor *)0);
		fill_om_desc(ref_attr[3], DS_MOD_TYPE, DS_ADD_ATTRIBUTE,
		    OM_S_ENUMERATION);
		fill_om_desc(ref_attr[4]);

		if (! ref_to_xds_attr_value(ref, &ref_attr[2])) {
			err = FN_E_MALFORMED_REFERENCE;
			delete [] ref_attr;
			ok = 0;
			fill_om_desc(mod_list[i + 1]); // zero it
		}
	}

	// convert to private object
	if (ok) {
		if (om_create(DS_C_ENTRY_MOD_LIST, OM_FALSE, workspace,
		    &pri_mod_list) == OM_SUCCESS) {
			om_put(pri_mod_list, OM_REPLACE_ALL, mod_list, 0, 0, 0);
		}
	}

	// cleanup
	ml = mod_list;
	if (ref && ok) {
		if (ref_attr[2].syntax == OM_S_OCTET_STRING)
			delete [] ref_attr[2].value.string.elements;
		delete [] ref_attr;
		fill_om_desc(mod_list[i + 1]); // zero it
	}
	ml++; // skip OM_CLASS
	while ((ml->type == DS_CHANGES) && (ml->type != OM_NO_MORE_TYPES)) {
		attribute = ml->value.object.object;
		attribute++;	// skip OM_CLASS
		while (attribute->type != OM_NO_MORE_TYPES) {
			if ((attribute->type == DS_ATTRIBUTE_TYPE) ||
			    (attribute->type == DS_ATTRIBUTE_VALUES)) {
				delete_xds_attr_value(attribute);
			}
			attribute++;
		}
		delete [] ml->value.object.object;
		ml++;
	}
	delete [] mod_list;

	return (ok ? pri_mod_list : 0);
}


/*
 * Convert an XFN attribute modification list to either an XDS entry
 * modification list or an XDS attribute list. An XDS attribute list
 * is constructed if 'attr_count' is zero.
 *
 * NOTE: The semantics of XFN and XDS modify operations do not match
 *       exactly. The entry to be modified must be pre-read and the
 *       XFN modifications adjusted accordingly.
 */
OM_private_object
XDSXOM::mods_to_xds_list(
	const FN_attrmodlist	&mods,
	OM_public_object	attr_list,
	unsigned int		attr_count,
	int			&err
) const
{
	int	mod_num;

	if ((mod_num = mods.count()) == 0) {
		err = FN_E_ATTR_VALUE_REQUIRED;
		return (0);
	}

	x500_trace("XDSXOM::mods_to_xds_entry_mod_list: %d XFN modify %s\n",
	    mod_num, (mod_num == 1) ? "operation" : "operations");

	OM_private_object	pri_mod_list = 0;
	OM_descriptor		*mod_list;
	OM_descriptor		*ml;
	OM_descriptor		*attribute;
	OM_descriptor		*dattribute = 0;
	OM_descriptor		*xattribute = 0;
	OM_descriptor		*xvalues = 0;
	const FN_attribute	*attr;
	const FN_attrvalue	*val;
	const FN_identifier	*id;
	void			*mod_iter;
	void			*val_iter;
	OM_string		oid;
	OM_syntax		syntax;
	FN_string		*val_str;
	unsigned int		mod_op;
	int			op;
	int			val_num;
	int			ok = 1;
	int			i;
	int			j;	// adjustment to index 'i'
	int			k;
	int			l;	// adjustment to index 'k'

	// if building an entry modification list then double 'mod_num' to
	//  cater for any replace ops (replace = delete + add)
	i = attr_count ? ((mod_num * 2) + 2) : (mod_num + 2);

	if ((mod_list = new OM_descriptor [i]) == 0) {
		err = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}

	// entry modification list or attribute list
	fill_om_desc(mod_list[0], OM_CLASS,
	    attr_count ? DS_C_ENTRY_MOD_LIST : DS_C_ATTRIBUTE_LIST);

	fill_om_desc(mod_list[1]); // zero it

	// examine each XFN modification
	for (i = 0, j = 0, attr = mods.first(mod_iter, mod_op);
	    (i < mod_num) && (ok);
	    attr = mods.next(mod_iter, mod_op), i++) {

		fill_om_desc(mod_list[i + j + 1]); // zero it

		val_num = attr->valuecount();
		if ((attribute = new OM_descriptor [val_num + 4]) == 0) {
			err = FN_E_INSUFFICIENT_RESOURCES;
			ok = 0;
			continue;
		}
		// entry modification list or attribute list
		fill_om_desc(mod_list[i + j + 1],
		    attr_count ? DS_CHANGES : DS_ATTRIBUTES, attribute);

		fill_om_desc(mod_list[i + j + 2]); // zero it
		fill_om_desc(attribute[0]); // zero it

		// extract attribute identifier
		id = attr->identifier();
		if ((! id) || (! id_to_om_oid(*id, oid, &syntax, 0))) {
			if (oid.elements)
				delete [] oid.elements;
			err = FN_E_INVALID_ATTR_IDENTIFIER;
			ok = 0;
			continue;
		}

		// check if attribute already exists in the entry
		if (mod_op != FN_ATTR_OP_ADD_EXCLUSIVE) {
			xattribute = locate_xds_attribute(&oid, attr_list,
			    attr_count);
		}

		// convert XFN modify operation to equivalent XDS operation(s)
		switch (mod_op) {

		case FN_ATTR_OP_ADD_EXCLUSIVE:
			// XFN add-exclusive -> XDS add-attribute

			op = DS_ADD_ATTRIBUTE;
			break;

		case FN_ATTR_OP_ADD:
			// if attribute already exists in X.500:
			//	XFN add -> XDS remove-atribute + add-attribute
			// if attribute does not exist in X.500:
			//	XFN add -> XDS add-attribute

			if (xattribute) {
				OM_string	doid;

				if ((dattribute = new OM_descriptor [4]) == 0) {
					err = FN_E_INSUFFICIENT_RESOURCES;
					ok = 0;
					continue;
				}
				doid.length = oid.length;
				if (! (doid.elements =
				    new unsigned char [oid.length])) {
					err = FN_E_INSUFFICIENT_RESOURCES;
					ok = 0;
					continue;
				}
				memcpy(doid.elements, oid.elements,
				    (size_t)oid.length);

				fill_om_desc(dattribute[0], OM_CLASS,
				    DS_C_ENTRY_MOD);
				fill_om_desc(dattribute[1], DS_ATTRIBUTE_TYPE,
				    doid);
				fill_om_desc(dattribute[2], DS_MOD_TYPE,
				    DS_REMOVE_ATTRIBUTE, OM_S_ENUMERATION);
				fill_om_desc(dattribute[3]);

				fill_om_desc(mod_list[i + j + 1], DS_CHANGES,
				    dattribute);
				j++;	// roll forward one
			}
			// entry modification list or attribute list
			fill_om_desc(mod_list[i + j + 1],
			    attr_count ? DS_CHANGES : DS_ATTRIBUTES,
			    attribute);
			op = DS_ADD_ATTRIBUTE;
			break;

		case FN_ATTR_OP_REMOVE:
			// if attribute already exists in X.500:
			//	XFN remove -> XDS remove-attribute
			// if attribute does not exist in X.500:
			//	skip this modification

			if (xattribute) {
				op = DS_REMOVE_ATTRIBUTE;
				break;
			} else {
				delete [] oid.elements;
				delete attribute;
				continue;
			}

		case FN_ATTR_OP_ADD_VALUES:
			// if attribute already exists in X.500:
			//	XFN add-values -> XDS add-values
			//	(only add those values which do not exist)
			// if attribute does not exist in X.500:
			//	XFN add-values -> XDS add-attribute

			if (xattribute)
				op = DS_ADD_VALUES;
			else
				op = DS_ADD_ATTRIBUTE;
			break;

		case FN_ATTR_OP_REMOVE_VALUES:
			// if attribute already exists in X.500:
			//	XFN remove-values -> XDS remove-values
			//	(only remove those values which already exist)
			// if attribute does not exist in X.500:
			//	skip this modification

			if (xattribute) {
				op = DS_REMOVE_VALUES;
				break;
			} else {
				delete [] oid.elements;
				delete attribute;
				continue;
			}

		default:
			delete [] oid.elements;
			delete attribute;
			err = FN_E_OPERATION_NOT_SUPPORTED;
			ok = 0;
			continue;
		}

		fill_om_desc(attribute[0], OM_CLASS, DS_C_ENTRY_MOD);
		fill_om_desc(attribute[1], DS_MOD_TYPE, op,
		    OM_S_ENUMERATION);
		fill_om_desc(attribute[2], DS_ATTRIBUTE_TYPE, oid);
		fill_om_desc(attribute[val_num + 3]);

		// locate start of existing attribute values
		if (xattribute) {
			xvalues = xattribute;
			while ((xvalues->type != DS_ATTRIBUTE_VALUES) &&
			    (xvalues->type != OM_NO_MORE_TYPES))
				xvalues++;
		}

		// handle attribute values
		for (k = 0, l = 0, val = attr->first(val_iter);
		    (k < val_num) && (ok);
		    val = attr->next(val_iter), k++) {

			attribute[k + l + 3].type = DS_ATTRIBUTE_VALUES;
			attribute[k + l + 3].syntax = syntax;

			val_str = val->string();
			if (! string_to_xds_attr_value(val_str, syntax, 0,
			    &attribute[k + l + 3])) {
				delete val_str;
				err = FN_E_INVALID_ATTR_VALUE;
				ok = 0;
				fill_om_desc(attribute[k + l + 3]); // zero it
				continue;
			}

			if ((op == DS_ADD_VALUES) || (op == DS_REMOVE_VALUES)) {

				int	present;

				// %%% test against current mods too
				present = locate_xds_attribute_value(
				    &attribute[k + l + 3], val_str, xvalues);

				if (present == -1) {	// error
					delete val_str;
					err = FN_E_OPERATION_NOT_SUPPORTED;
					ok = 0;
					fill_om_desc(attribute[k + l + 3]);
					continue;
				}

				// skip this value if it already exists in X.500
				if (present && (op == DS_ADD_VALUES)) {
					delete val_str;
					fill_om_desc(attribute[k + l + 3]);
					l--;	// roll back one
					continue;
				}

				// skip this value if it does not exist in X.500
				if ((! present) && (op == DS_REMOVE_VALUES)) {
					delete val_str;
					fill_om_desc(attribute[k + l + 3]);
					l--;	// roll back one
					continue;
				}
			}
		}
		if ((ok) && ((k + l) == 0) &&
		    ((op == DS_ADD_VALUES) || (op == DS_REMOVE_VALUES))) {
			// no values present so undo current modification

			fill_om_desc(mod_list[i + j + 1]); // zero it
			delete [] oid.elements;
			delete attribute;
			j--;	// roll back one
		}
	}
	if (ok)
		fill_om_desc(mod_list[i + j + 1]); // zero it

	// convert to private object
	if (ok) {
		if (om_create(
		    attr_count ? DS_C_ENTRY_MOD_LIST : DS_C_ATTRIBUTE_LIST,
		    OM_FALSE, workspace, &pri_mod_list) == OM_SUCCESS) {
			om_put(pri_mod_list, OM_REPLACE_ALL, mod_list, 0, 0, 0);
		}
	}

	// cleanup
	ml = mod_list;
	ml++; // skip OM_CLASS
	while ((ml->type == (attr_count ? DS_CHANGES : DS_ATTRIBUTES)) &&
	    (ml->type != OM_NO_MORE_TYPES)) {
		attribute = ml->value.object.object;
		attribute++;	// skip OM_CLASS
		while (attribute->type != OM_NO_MORE_TYPES) {
			if ((attribute->type == DS_ATTRIBUTE_TYPE) ||
			    (attribute->type == DS_ATTRIBUTE_VALUES)) {
				delete_xds_attr_value(attribute);
			}
			attribute++;
		}
		delete [] ml->value.object.object;
		ml++;
	}
	delete [] mod_list;

	return (ok ? pri_mod_list : 0);
}


/*
 * locate the specified attribute identifier in an array of attributes
 */
OM_public_object
XDSXOM::locate_xds_attribute(
	OM_string		*oid,
	OM_public_object	attr_list,
	int			attr_num
) const
{
	OM_descriptor	*al = attr_list;
	OM_descriptor	*a;
	int		i;

	for (i = 0; i < attr_num; i++) {
		a = al[i].value.object.object;
		a++; // skip OM_CLASS

		while ((a->type != DS_ATTRIBUTE_TYPE) &&
		    (a->type != OM_NO_MORE_TYPES))
			a++;

		if ((a->type == DS_ATTRIBUTE_TYPE) &&
		    (compare_om_oids(*oid, a->value.string)))
			return (a);
	}
	return (0);
}


/*
 * locate the specified attribute value in an array of attribute values
 */
int
XDSXOM::locate_xds_attribute_value(
	OM_descriptor	*value,
	FN_string	*value_string,
	OM_descriptor	*value_list
) const
{
	OM_descriptor	*vl;
	OM_string	*s = &value->value.string;
	OM_syntax	syntax = value->syntax;

	for (vl = value_list;
	    ((vl->type == DS_ATTRIBUTE_VALUES) &&
	    (vl->type != OM_NO_MORE_TYPES));
	    vl++) {

		if (syntax != (vl->syntax & OM_S_SYNTAX))
			continue;

		if (syntax == OM_S_OBJECT) {
			OM_string	xs;
			unsigned int	format;
			unsigned int	length;

			// convert XDS value to XFN string format
			if (! (xs.elements =
			    xds_attr_value_to_string(vl, format, length))) {
				return (-1);	// error
			}

			// compare strings
			if ((length == value_string->bytecount()) &&
			    (memcmp(value_string->str(), xs.elements, length) ==
			    0)) {
				delete [] xs.elements;
					// xds_attr_value_to_string() allocates
				return (1);	// matched
			}
			delete [] xs.elements;
				// xds_attr_value_to_string() allocates

		} else if ((syntax == OM_S_ENUMERATION) ||
		    (syntax == OM_S_INTEGER) || (syntax == OM_S_BOOLEAN)) {

			if (value->value.enumeration == vl->value.enumeration)
				return (1);	// matched
		} else {
			// various strings
			if ((s->length == vl->value.string.length) &&
			    (memcmp(s->elements, vl->value.string.elements,
			    (size_t)vl->value.string.length) == 0))

				return (1);	// matched
		}
	}
	return (0);
}





/*
 * convert one or more attributes into XDS filter object
 */
OM_private_object
XDSXOM::attrs_to_xds_filter(
	const FN_attrset	*attrs,
	int			&ok
) const
{
	int	attr_num;
	int	attr_val_num;

	if ((! attrs) || ((attr_num = attrs->count()) == 0))
		return (DS_NO_FILTER);

	x500_trace("XDSXOM::attrs_to_xds_filter: %d %s\n", attr_num,
	    "XFN filter attribute(s)");

	OM_private_object	pri_filter = 0;
	OM_descriptor		*filter;
	OM_descriptor		*filter_items;
	OM_descriptor		*filter_item;
	const FN_attribute	*attr;
	const FN_attrvalue	*val;
	const FN_identifier	*id;
	void			*attr_iter;
	void			*val_iter;
	OM_string		oid;
	OM_syntax		syntax;
	FN_string		*val_str;
	int			num = attr_num * 5;
	int			i;

	if ((filter = new OM_descriptor [attr_num + 3]) == 0) {
		ok = 0;
		return (0);
	}
	if ((filter_items = new OM_descriptor [attr_num * 5]) == 0) {
		ok = 0;
		delete [] filter;
		return (0);
	}
	filter_item = filter_items;

	fill_om_desc(filter[0], OM_CLASS, DS_C_FILTER);
	fill_om_desc(filter[1], DS_FILTER_TYPE, DS_AND, OM_S_ENUMERATION);
	fill_om_desc(filter[attr_num + 2]);

	for (attr = attrs->first(attr_iter);
	    (attr_num > 0) && (ok);
	    attr = attrs->next(attr_iter), attr_num--) {

		fill_om_desc(filter[attr_num + 1], DS_FILTER_ITEMS,
		    filter_item);

		id = attr->identifier();
		if ((! id) || (! id_to_om_oid(*id, oid, &syntax, 0))) {
			if (oid.elements)
				delete [] oid.elements;
			ok = 0;
			continue;
		}
		fill_om_desc(*filter_item++, OM_CLASS, DS_C_FILTER_ITEM);

		switch (attr_val_num = attr->valuecount()) {

		case 0:
			fill_om_desc(*filter_item++, DS_FILTER_ITEM_TYPE,
			    DS_PRESENT, OM_S_ENUMERATION);
			fill_om_desc(*filter_item++, DS_ATTRIBUTE_TYPE, oid);
			break;

		case 1:
			fill_om_desc(*filter_item++, DS_FILTER_ITEM_TYPE,
			    DS_EQUALITY, OM_S_ENUMERATION);

			fill_om_desc(*filter_item++, DS_ATTRIBUTE_TYPE, oid);
			val = attr->first(val_iter);
			filter_item->type = DS_ATTRIBUTE_VALUES;
			filter_item->syntax = syntax;

			val_str = val->string();
			if (! string_to_xds_attr_value(val_str, syntax, 0,
			    filter_item)) {
				delete val_str;
				ok = 0;
			}
			filter_item++;
			break;

		default:
			ok = 0;
			break;
		}
		fill_om_desc(*filter_item++);
	}

	// convert to private object
	if (ok) {
		if (om_create(DS_C_FILTER, OM_FALSE, workspace, &pri_filter) ==
		    OM_SUCCESS) {
			om_put(pri_filter, OM_REPLACE_ALL, filter, 0, 0, 0);
		}
	}

	for (i = 0; i < num; i++) {
		if ((filter_items[i].type == DS_ATTRIBUTE_TYPE) ||
		    (filter_items[i].type == DS_ATTRIBUTE_VALUES)) {
			delete_xds_attr_value(&filter_items[i]);
		}
	}
	delete [] filter_items;
	delete [] filter;

	return (ok ? pri_filter : 0);
}


/*
 * Convert an LDAP filter string into an XDS filter item object
 */
OM_public_object
XDSXOM::string_to_xds_filter_item(
	const unsigned char	*lfilter,
	int			&err
) const
{
	OM_descriptor		*x;
	OM_descriptor		*xfitem;
	OM_enumeration		fitem_type;
	OM_object_identifier	*oid;
	OM_object_identifier	*class_oid;
	OM_syntax		syntax;
	const unsigned char	*opd;	// operand
	const unsigned char	*lhs;	// LHS of operand
	unsigned char		*lhs_str;
	const unsigned char	*op;	// operator
	const unsigned char	*rhs;	// RHS of operand
	unsigned char		*rhs_str;
	unsigned char		*ss;	// substring
	unsigned char		*cp;
	size_t			len;
	int			initial;
	int			final;
	int			wildcard;

	// operand: LHS, operator, RHS

	opd = lfilter;
	if (*opd == '(')
		opd++;
	lhs = opd;
	while (*opd != ')') {

		if (*opd == '=') {

			fitem_type = DS_EQUALITY;
				// or DS_PRESENT
				// or DS_SUBSTRINGS
			op = opd;
			opd++;
			break;

		} else if ((*opd == '~') && (*(opd + 1) == '=')) {

			fitem_type = DS_APPROXIMATE_MATCH;
			op = opd;
			opd += 2;
			break;

		} else if ((*opd == '>') && (*(opd + 1) == '=')) {

			fitem_type = DS_GREATER_OR_EQUAL;
			op = opd;
			opd += 2;
			break;

		} else if ((*opd == '<') && (*(opd + 1) == '=')) {

			fitem_type = DS_LESS_OR_EQUAL;
			op = opd;
			opd += 2;
			break;
		}
		opd++;
	}
	rhs = opd;
	wildcard = 0;
	if (*rhs == '*')
		initial = 0;
	else
		initial = 1;

	while (*opd != ')') {
		if (*opd == '*')
			wildcard++;
		opd++;
	}

	if (*(opd - 1) == '*')
		final = 0;
	else
		final = 1;

	// copy LHS
	len = op - lhs;
	if (! (lhs_str = new unsigned char [len + 1])) {
		err = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	memcpy(lhs_str, lhs, len);
	lhs_str[len] = '\0';

	// copy RHS
	len = opd - rhs;
	if ((len == 1) && (wildcard)) {
		rhs_str = 0;
		fitem_type = DS_PRESENT;
	} else {
		if (wildcard)
			fitem_type = DS_SUBSTRINGS;

		if (! (rhs_str = new unsigned char [len + 1])) {
			delete [] lhs_str;
			err = FN_E_INSUFFICIENT_RESOURCES;
			return (0);
		}
		memcpy(rhs_str, rhs, len);
		rhs_str[len] = '\0';
	}

	if (! (x = xfitem = new OM_descriptor [5 + wildcard])) {
		delete [] lhs_str;
		delete [] rhs_str;
		err = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}

	// build DS_C_FILTER_ITEM object
	fill_om_desc(*xfitem++, OM_CLASS, DS_C_FILTER_ITEM);
	fill_om_desc(*xfitem++, DS_FILTER_ITEM_TYPE, fitem_type,
	    OM_S_ENUMERATION);

	// set attribute identifier and syntax
	if (oid = string_to_om_oid(lhs_str)) {

		fill_om_desc(*xfitem++, DS_ATTRIBUTE_TYPE, *oid);
		delete [] lhs_str;

	} else if (oid = string_oid_to_om_oid((char *)lhs_str)) {

		fill_om_desc(*xfitem++, DS_ATTRIBUTE_TYPE, *oid);
		delete [] lhs_str;

	} else {
		delete [] lhs_str;
		delete [] rhs_str;
		delete [] xfitem;
		err = FN_E_INVALID_ATTR_IDENTIFIER;

		x500_trace("XDSXOM::string_to_xds_filter_item: %s\n",
		    "cannot map string to an object identifier");

		return (0);
	}
	delete [] lhs_str;
	if (! (syntax = om_oid_to_syntax(oid, &syntax, &class_oid))) {
		syntax = OM_S_TELETEX_STRING;	// default string syntax

		x500_trace("XDSXOM::string_to_xds_filter_item: %s - %s\n",
		    "unrecognised attribute", "using teletex-string syntax");
	}

	// set attribute value(s)
	if ((fitem_type == DS_EQUALITY) ||
	    (fitem_type == DS_APPROXIMATE_MATCH) ||
	    (fitem_type == DS_GREATER_OR_EQUAL) ||
	    (fitem_type == DS_LESS_OR_EQUAL)) {

		xfitem->type = DS_ATTRIBUTE_VALUES;
		xfitem->syntax = syntax;

		FN_string	rhs_string(rhs_str);

		if (! string_to_xds_attr_value(&rhs_string, syntax, class_oid,
		    xfitem)) {
			delete [] oid->elements;
			delete oid;
			delete [] rhs_str;
			delete [] xfitem;
			err = FN_E_INVALID_ATTR_VALUE;

			return (0);
		}
		xfitem++;

	} else if (fitem_type == DS_SUBSTRINGS) {

		cp = rhs_str;

		// initial substring
		if (initial) {
			ss = cp;
			while (*cp != '*')
				cp++;
			*cp++ = '\0';
			fill_om_desc(*xfitem++, DS_INITIAL_SUBSTRING, syntax,
			    ss);
		}

		while (--wildcard) {
			ss = cp;
			while (*cp != '*')
				cp++;
			*cp++ = '\0';

			xfitem->type = DS_ATTRIBUTE_VALUES;
			xfitem->type = syntax;

			FN_string	ss_string(ss);

			if (! string_to_xds_attr_value(&ss_string, syntax,
			    class_oid, xfitem)) {
				delete [] oid->elements;
				delete oid;
				delete [] rhs_str;
				delete [] xfitem;
				err = FN_E_INVALID_ATTR_VALUE;

				return (0);
			}
			xfitem++;
		}

		// final substring
		if (final) {
			ss = cp;
			while (*cp)
				cp++;
			fill_om_desc(*xfitem++, DS_FINAL_SUBSTRING, syntax, ss);
		}
	}
	fill_om_desc(*xfitem++);	// zero it
	delete oid;	// delete contents later

	return (x);
}


/*
 * Convert an LDAP filter string into an XDS filter object
 */
OM_public_object
XDSXOM::string_to_xds_filter(
	const unsigned char	*lfilter,
	OM_public_object	xfilter,
	int			&err
) const
{
	OM_descriptor	*x;	// start of an XDS filter or filter-item

	while (*lfilter) {

		if ((*lfilter == '&') || (*lfilter == '|')) {

			// build DS_C_FILTER object
			x = xfilter;
			fill_om_desc(*xfilter++, OM_CLASS, DS_C_FILTER);
			fill_om_desc(*xfilter++, DS_FILTER_TYPE,
			    (*lfilter == '&') ? DS_AND : DS_OR,
			    OM_S_ENUMERATION);
			fill_om_desc(*xfilter);	// zero it

			// build first operand
			if (! (x = string_to_xds_filter(lfilter, xfilter + 3,
			    err))) {
				return (0);
			}
			// XDS filter or filter-item
			if ((x + 1)->type == DS_FILTER_TYPE) {
				fill_om_desc(*xfilter++, DS_FILTERS, x);
			} else {
				fill_om_desc(*xfilter++, DS_FILTER_ITEMS, x);
			}

			// build second operand
			if (! (x = string_to_xds_filter(lfilter, xfilter + 3,
			    err))) {
				return (0);
			}
			// XDS filter or filter-item
			if ((x + 1)->type == DS_FILTER_TYPE) {
				fill_om_desc(*xfilter++, DS_FILTERS, x);
			} else {
				fill_om_desc(*xfilter++, DS_FILTER_ITEMS, x);
			}
			fill_om_desc(*xfilter++);	// zero it

		} else if (*lfilter == '!') {

			// build DS_C_FILTER object
			x = xfilter;
			fill_om_desc(*xfilter++, OM_CLASS, DS_C_FILTER);
			fill_om_desc(*xfilter++, DS_FILTER_TYPE, DS_NOT,
			    OM_S_ENUMERATION);
			fill_om_desc(*xfilter);	// zero it

			// build first operand
			if (! (x = string_to_xds_filter(lfilter, xfilter + 2,
				err))) {
				return (0);
			}
			// XDS filter or filter-item
			if ((x + 1)->type == DS_FILTER_TYPE) {
				fill_om_desc(*xfilter++, DS_FILTERS, x);
			} else {
				fill_om_desc(*xfilter++, DS_FILTER_ITEMS, x);
			}
			fill_om_desc(*xfilter++);	// zero it

		} else {
			// operand

			return (string_to_xds_filter_item(lfilter, err));
		}
		fill_om_desc(*xfilter++);	// zero it
	}

	return (x);
}


/*
 * Convert an XFN filter into an XDS filter object
 *
 * 1. convert XFN filter to LDAP format.
 * 2. convert LDAP format to XDS filter object.
 */
OM_private_object
XDSXOM::filter_to_xds_filter(
	const FN_search_filter	*filter,
	int			&err
)
{
	const unsigned char	*fx;		// XFN filter expression
	unsigned char		*lfilter;	// LDAP filter
	unsigned char		*cp;
	int			ops = 0;
	OM_descriptor		*xfilter;	// XDS filter
	OM_private_object	pri_fil = 0;

	if ((! filter) || ((fx = filter->filter_expression()) && (! *fx))) {
		x500_trace("XDSXOM::filter_to_xds_filter: no filter\n");
		return (DS_NO_FILTER);
	}

	if (! fx) {
		err = FN_E_SEARCH_INVALID_FILTER;
		return (0);
	}

	// convert filter to LDAP filter string
	if (! (lfilter = filter_to_string(filter, err))) {
		return (0);
	}

	// build XDS filter object

	// count operators
	cp = lfilter;
	while (*cp) {
		if ((*cp == '|') || (*cp == '&') || (*cp == '!'))
			ops++;
		cp++;
	}
	if (! ops)
		ops = 1;

	// overestimate the space required
	if (! (xfilter = new OM_descriptor [ops * 5])) {
		delete [] lfilter;
		err = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}

	if (string_to_xds_filter(lfilter, xfilter, err)) {
		// convert to private object
		if (om_create(DS_C_FILTER, OM_FALSE, workspace, &pri_fil) ==
		    OM_SUCCESS) {
			om_put(pri_fil, OM_REPLACE_ALL, xfilter, 0, 0, 0);
		}
	} else {
		pri_fil = 0;
	}

	// cleanup
	OM_descriptor	*d = xfilter;
	OM_descriptor	*fi;

	while (d->type != OM_NO_MORE_TYPES) {
		if (d->type == DS_FILTER_ITEMS) {
			fi = d->value.object.object;
			while (fi->type != OM_NO_MORE_TYPES) {
				if (fi->type == DS_ATTRIBUTE_TYPE)
					delete [] fi->value.string.elements;

				// all substring values point into this buffer
				if (fi->type == DS_INITIAL_SUBSTRING)
					delete [] fi->value.string.elements;

				fi++;
			}
			delete [] d->value.object.object;
		}
		d++;
	}
	delete [] xfilter;
	delete [] lfilter;

	return (pri_fil);
}


/*
 * extract components from XFN search control
 */
int
XDSXOM::extract_search_controls(
	const FN_search_control	*control,
	unsigned int		authoritative,
	OM_private_object	&new_context,
	OM_sint			&scope,
	OM_private_object	&selection,
	unsigned int		&return_ref,
	int			&err
) const
{
	int			use_local_scope = 0;
	int			use_size_limit = 0;
	int			ok = 1;
	unsigned int		max_names;
	static OM_descriptor	local_scope_mod[] = {
		{DS_LOCAL_SCOPE, OM_S_BOOLEAN, {{OM_TRUE, NULL}}},
		OM_NULL_DESCRIPTOR
	};

	if (control) {

		// set the scope of the search
		switch (control->scope()) {
		case FN_SEARCH_SUBTREE:
			scope = DS_WHOLE_SUBTREE;
			x500_trace("XDSXOM::extract_search_controls: %s\n",
			    "scope: subtree");
			break;

		case FN_SEARCH_CONSTRAINED_SUBTREE:
			scope = DS_WHOLE_SUBTREE;
			use_local_scope = 1;
			x500_trace("XDSXOM::extract_search_controls: %s\n",
			    "scope: subtree");
			break;

		case FN_SEARCH_ONE_CONTEXT:
			scope = DS_ONE_LEVEL;
			x500_trace("XDSXOM::extract_search_controls: %s\n",
			    "scope: one-level");
			break;

		case FN_SEARCH_NAMED_OBJECT:
			scope = DS_BASE_OBJECT;
			x500_trace("XDSXOM::extract_search_controls: %s\n",
			    "scope: base");
			break;

		default:
			err = FN_E_SEARCH_INVALID_OPTION;
			return (0);
		}

		// set size limit for the search
		if (max_names = control->max_names()) {
			use_size_limit = 1;
			x500_trace("XDSXOM::extract_search_controls: %s: %d\n",
			    "size limit", max_names);
		} else {
			x500_trace("XDSXOM::extract_search_controls: %s\n",
			    "no size limit");
		}

		// return references ?
		return_ref = control->return_ref();

		// return attributes ?
		selection = ids_to_xds_selection(control->return_attr_ids(),
		    return_ref, ok);
		if (! ok) {
			err = FN_E_INVALID_ATTR_IDENTIFIER;
			return (0);
		}

	} else {
		scope = DS_ONE_LEVEL;
		selection = DS_SELECT_NO_ATTRIBUTES;

		x500_trace("XDSXOM::extract_search_controls: defaults\n");
	}

	if (use_local_scope || use_size_limit) {

		if (authoritative) {
			if (om_copy(auth_context, workspace, &new_context) !=
			    OM_SUCCESS) {
				err = FN_E_INSUFFICIENT_RESOURCES;
				return (0);
			}
		} else {
			if (om_create(DS_C_CONTEXT, OM_TRUE, workspace,
			    &new_context) != OM_SUCCESS) {
				err = FN_E_INSUFFICIENT_RESOURCES;
				return (0);
			}
		}

		// set local-scope flag
		if (use_local_scope) {
			if (om_put(new_context, OM_REPLACE_ALL, local_scope_mod,
			    0, 0, 0) != OM_SUCCESS) {
				err = FN_E_INSUFFICIENT_RESOURCES;
				return (0);
			}
		}

		// set size-limit
		if (use_size_limit) {
			OM_descriptor	size_limit_mod[2];

			fill_om_desc(size_limit_mod[0], DS_SIZE_LIMIT,
			    max_names, OM_S_INTEGER);
			fill_om_desc(size_limit_mod[1]);

			if (om_put(new_context, OM_REPLACE_ALL, size_limit_mod,
			    0, 0, 0) != OM_SUCCESS) {
				err = FN_E_INSUFFICIENT_RESOURCES;
				return (0);
			}
		}
	}
	return (1);
}
