/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)XDSInfo.cc	1.2	97/11/07 SMI"


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>	// vfprintf()
#include "XDSInfo.hh"
#include "XDSExtern.hh"

extern "C" {

#include "xds.h"
#include "xdsbdcp.h"
#include "xdsmdup.h"
#include "xdssap.h"
#include "xdsxfnp.h"

}


/*
 * XDS information tables and data structures
 */


// object identifier (dotted decimal or ASN.1 BER form)
const int	XDSInfo::max_oid_length = 128;


#define	OM_S_OBJECT_ID_STRING	OM_S_OBJECT_IDENTIFIER_STRING

const struct oid_to_string_t	XDSInfo::oid_to_string_table[] = {

// this table is ordered on its first field
// (its second field is an index into the string_to_oid_table)
//
// XDS object id , index ,  [ XOM syntax ] , [ XOM class object id ]

{&DS_O_XFN,			91, 0,				0},
{&DS_O_XFN_SUPPLEMENT,		93, 0,				0},
{&DS_A_OBJECT_CLASS,		33, OM_S_OBJECT_ID_STRING,	0},
{&DS_A_ALIASED_OBJECT_NAME,	 2, OM_S_OBJECT, &DS_C_DS_DN},
{&DS_A_KNOWLEDGE_INFO,		27, OM_S_TELETEX_STRING,	0},
{&DS_A_COMMON_NAME,		10, OM_S_TELETEX_STRING,	0},
{&DS_A_SURNAME,			77, OM_S_TELETEX_STRING,	0},
{&DS_A_SERIAL_NBR,		70, OM_S_PRINTABLE_STRING,	0},
{&DS_A_COUNTRY_NAME,		13, OM_S_PRINTABLE_STRING,	0},
{&DS_A_LOCALITY_NAME,		30, OM_S_TELETEX_STRING,	0},
{&DS_A_STATE_OR_PROV_NAME,	72, OM_S_TELETEX_STRING,	0},
{&DS_A_STREET_ADDRESS,		74, OM_S_TELETEX_STRING,	0},
{&DS_A_ORG_NAME,		44, OM_S_TELETEX_STRING,	0},
{&DS_A_ORG_UNIT_NAME,		43, OM_S_TELETEX_STRING,	0},
{&DS_A_TITLE,			84, OM_S_TELETEX_STRING,	0},
{&DS_A_DESCRIPTION,		14, OM_S_TELETEX_STRING,	0},
{&DS_A_SEARCH_GUIDE,		66, OM_S_OBJECT, &DS_C_SEARCH_GUIDE},
{&DS_A_BUSINESS_CATEGORY,	 8, OM_S_TELETEX_STRING,	0},
{&DS_A_POSTAL_ADDRESS,		52, OM_S_OBJECT, &DS_C_POSTAL_ADDRESS},
{&DS_A_POSTAL_CODE,		53, OM_S_TELETEX_STRING,	0},
{&DS_A_POST_OFFICE_BOX,		54, OM_S_TELETEX_STRING,	0},
{&DS_A_PHYS_DELIV_OFF_NAME,	48, OM_S_TELETEX_STRING,	0},
{&DS_A_PHONE_NBR,		79, OM_S_PRINTABLE_STRING,	0},
{&DS_A_TELEX_NBR,		83, OM_S_OBJECT, &DS_C_TELEX_NBR},
{&DS_A_TELETEX_TERM_IDENT,	81, OM_S_OBJECT, &DS_C_TELETEX_TERM_IDENT},
{&DS_A_FACSIMILE_PHONE_NBR,	21, OM_S_OBJECT, &DS_C_FACSIMILE_PHONE_NBR},
{&DS_A_X121_ADDRESS,		89, OM_S_NUMERIC_STRING,	0},
{&DS_A_INTERNAT_ISDN_NBR,	25, OM_S_NUMERIC_STRING,	0},
{&DS_A_REGISTERED_ADDRESS,	60, OM_S_OBJECT, &DS_C_POSTAL_ADDRESS},
{&DS_A_DEST_INDICATOR,		16, OM_S_PRINTABLE_STRING,	0},
{&DS_A_PREF_DELIV_METHOD,	56, OM_S_ENUMERATION,		0},
{&DS_A_PRESENTATION_ADDRESS,	58, OM_S_OBJECT, &DS_C_PRESENTATION_ADDRESS},
{&DS_A_SUPPORT_APPLIC_CONTEXT,	76, OM_S_OBJECT_ID_STRING,	0},
{&DS_A_MEMBER,			31, OM_S_OBJECT, &DS_C_DS_DN},
{&DS_A_OWNER,			45, OM_S_OBJECT, &DS_C_DS_DN},
{&DS_A_ROLE_OCCUPANT,		64, OM_S_OBJECT, &DS_C_DS_DN},
{&DS_A_SEE_ALSO,		68, OM_S_OBJECT, &DS_C_DS_DN},
{&DS_A_USER_PASSWORD,		87, OM_S_OCTET_STRING,		0},
{&DS_O_TOP,			85, 0,				0},
{&DS_O_ALIAS,			 0, 0,				0},
{&DS_O_COUNTRY,			11, 0,				0},
{&DS_O_LOCALITY,		28, 0,				0},
{&DS_O_ORG,			34, 0,				0},
{&DS_O_ORG_UNIT,		42, 0,				0},
{&DS_O_PERSON,			46, 0,				0},
{&DS_O_ORG_PERSON,		40, 0,				0},
{&DS_O_ORG_ROLE,		41, 0,				0},
{&DS_O_GROUP_OF_NAMES,		23, 0,				0},
{&DS_O_RESIDENTIAL_PERSON,	62, 0,				0},
{&DS_O_APPLIC_PROCESS,		 6, 0,				0},
{&DS_O_APPLIC_ENTITY,		 5, 0,				0},
{&DS_O_DSA,			19, 0,				0},
{&DS_O_DEVICE,			17, 0,				0}

};
#undef OM_S_OBJECT_ID_STRING

const int	XDSInfo::oid_to_string_table_size =
		    sizeof (oid_to_string_table) / sizeof (oid_to_string_t);


const struct string_to_oid_t	XDSInfo::string_to_oid_table[] = {

// this table is ordered on its first field (case is ignored)
//
// string , [ abbreviation ] , XDS object id

/* 00 */ { "alias",			0,	&DS_O_ALIAS },
/* 01 */ { "aliased-object-name",	0,	&DS_A_ALIASED_OBJECT_NAME },
/* 02 */ { "aliasedObjectName",		0,	&DS_A_ALIASED_OBJECT_NAME },
/* 03 */ { "application-entity",	0, 	&DS_O_APPLIC_ENTITY },
/* 04 */ { "application-process",	0,	&DS_O_APPLIC_PROCESS },
/* 05 */ { "applicationEntity",		0, 	&DS_O_APPLIC_ENTITY },
/* 06 */ { "applicationProcess",	0,	&DS_O_APPLIC_PROCESS },
/* 07 */ { "business-category",		0,	&DS_A_BUSINESS_CATEGORY },
/* 08 */ { "businessCategory",		0,	&DS_A_BUSINESS_CATEGORY },
/* 09 */ { "common-name",		"CN",	&DS_A_COMMON_NAME },
/* 10 */ { "commonName",		"CN",	&DS_A_COMMON_NAME },
/* 11 */ { "country",			0,	&DS_O_COUNTRY },
/* 12 */ { "country-name",		"C",	&DS_A_COUNTRY_NAME },
/* 13 */ { "countryName",		"C",	&DS_A_COUNTRY_NAME },
/* 14 */ { "description",		0,	&DS_A_DESCRIPTION },
/* 15 */ { "destination-indicator",	0,	&DS_A_DEST_INDICATOR },
/* 16 */ { "destinationIndicator",	0,	&DS_A_DEST_INDICATOR },
/* 17 */ { "device",			0,	&DS_O_DEVICE },
/* 18 */ { "DSA",			0,	&DS_O_DSA },
/* 19 */ { "dSA",			0,	&DS_O_DSA },
/* 20 */ { "facsimile-telephone-number", 0,	&DS_A_FACSIMILE_PHONE_NBR },
/* 21 */ { "facsimileTelephoneNumber",	0,	&DS_A_FACSIMILE_PHONE_NBR },
/* 22 */ { "group-of-names",		0,	&DS_O_GROUP_OF_NAMES },
/* 23 */ { "groupOfNames",		0,	&DS_O_GROUP_OF_NAMES },
/* 24 */ { "international-ISDN-number",	0,	&DS_A_INTERNAT_ISDN_NBR },
/* 25 */ { "internationalISDNNumber",	0,	&DS_A_INTERNAT_ISDN_NBR },
/* 26 */ { "knowledge-information",	0,	&DS_A_KNOWLEDGE_INFO },
/* 27 */ { "knowledgeInformation",	0,	&DS_A_KNOWLEDGE_INFO },
/* 28 */ { "locality",			0,	&DS_O_LOCALITY },
/* 29 */ { "locality-name",		"L",	&DS_A_LOCALITY_NAME },
/* 30 */ { "localityName",		"L",	&DS_A_LOCALITY_NAME },
/* 31 */ { "member",			0,	&DS_A_MEMBER },
/* 32 */ { "object-class",		0,	&DS_A_OBJECT_CLASS },
/* 33 */ { "objectClass",		0,	&DS_A_OBJECT_CLASS },
/* 34 */ { "organization",		0,	&DS_O_ORG },
/* 35 */ { "organization-name",		"O",	&DS_A_ORG_NAME },
/* 36 */ { "organizational-person",	0,	&DS_O_ORG_PERSON },
/* 37 */ { "organizational-role",	0,	&DS_O_ORG_ROLE },
/* 38 */ { "organizational-unit",	0,	&DS_O_ORG_UNIT },
/* 39 */ { "organizational-unit-name",	"OU",	&DS_A_ORG_UNIT_NAME },
/* 40 */ { "organizationalPerson",	0,	&DS_O_ORG_PERSON },
/* 41 */ { "organizationalRole",	0,	&DS_O_ORG_ROLE },
/* 42 */ { "organizationalUnit",	0,	&DS_O_ORG_UNIT },
/* 43 */ { "organizationalUnitName",	"OU",	&DS_A_ORG_UNIT_NAME },
/* 44 */ { "organizationName",		"O",	&DS_A_ORG_NAME },
/* 45 */ { "owner",			0,	&DS_A_OWNER },
/* 46 */ { "person",			0,	&DS_O_PERSON },
/* 47 */ { "physical-delivery-office-name", 0,	&DS_A_PHYS_DELIV_OFF_NAME },
/* 48 */ { "physicalDeliveryOfficeName", 0,	&DS_A_PHYS_DELIV_OFF_NAME },
/* 49 */ { "post-office-box",		0,	&DS_A_POST_OFFICE_BOX },
/* 50 */ { "postal-address",		0,	&DS_A_POSTAL_ADDRESS },
/* 51 */ { "postal-code",		0,	&DS_A_POSTAL_CODE },
/* 52 */ { "postalAddress",		0,	&DS_A_POSTAL_ADDRESS },
/* 53 */ { "postalCode",		0,	&DS_A_POSTAL_CODE },
/* 54 */ { "postOfficeBox",		0,	&DS_A_POST_OFFICE_BOX },
/* 55 */ { "preferred-delivery-method",	0,	&DS_A_PREF_DELIV_METHOD },
/* 56 */ { "preferredDeliveryMethod",	0,	&DS_A_PREF_DELIV_METHOD },
/* 57 */ { "presentation-address",	0,	&DS_A_PRESENTATION_ADDRESS },
/* 58 */ { "presentationAddress",	0,	&DS_A_PRESENTATION_ADDRESS },
/* 59 */ { "registered-address",	0,	&DS_A_REGISTERED_ADDRESS },
/* 60 */ { "registeredAddress",		0,	&DS_A_REGISTERED_ADDRESS },
/* 61 */ { "residential-person",	0,	&DS_O_RESIDENTIAL_PERSON },
/* 62 */ { "residentialPerson",		0,	&DS_O_RESIDENTIAL_PERSON },
/* 63 */ { "role-occupant",		0,	&DS_A_ROLE_OCCUPANT },
/* 64 */ { "roleOccupant",		0,	&DS_A_ROLE_OCCUPANT },
/* 65 */ { "search-guide",		0,	&DS_A_SEARCH_GUIDE },
/* 66 */ { "searchGuide",		0,	&DS_A_SEARCH_GUIDE },
/* 67 */ { "see-also",			0,	&DS_A_SEE_ALSO },
/* 68 */ { "seeAlso",			0,	&DS_A_SEE_ALSO },
/* 69 */ { "serial-number",		0,	&DS_A_SERIAL_NBR },
/* 70 */ { "serialNumber",		0,	&DS_A_SERIAL_NBR },
/* 71 */ { "state-or-province-name",	"ST",	&DS_A_STATE_OR_PROV_NAME },
/* 72 */ { "stateOrProvinceName",	"ST",	&DS_A_STATE_OR_PROV_NAME },
/* 73 */ { "street-address",		0,	&DS_A_STREET_ADDRESS },
/* 74 */ { "streetAddress",		0,	&DS_A_STREET_ADDRESS },
/* 75 */ { "supported-application-context", 0,	&DS_A_SUPPORT_APPLIC_CONTEXT },
/* 76 */ { "supportedApplicationContext", 0,	&DS_A_SUPPORT_APPLIC_CONTEXT },
/* 77 */ { "surname",			"SN",	&DS_A_SURNAME },
/* 78 */ { "telephone-number",		0,	&DS_A_PHONE_NBR },
/* 79 */ { "telephoneNumber",		0,	&DS_A_PHONE_NBR },
/* 80 */ { "teletex-terminal-identifier", 0,	&DS_A_TELETEX_TERM_IDENT },
/* 81 */ { "teletexTerminalIdentifier", 0,	&DS_A_TELETEX_TERM_IDENT },
/* 82 */ { "telex-number",		0,	&DS_A_TELEX_NBR },
/* 83 */ { "telexNumber",		0,	&DS_A_TELEX_NBR },
/* 84 */ { "title",			0,	&DS_A_TITLE },
/* 85 */ { "top",			0,	&DS_O_TOP },
/* 86 */ { "user-password",		0,	&DS_A_USER_PASSWORD },
/* 87 */ { "userPassword",		0,	&DS_A_USER_PASSWORD },
/* 88 */ { "X121-address",		0,	&DS_A_X121_ADDRESS },
/* 89 */ { "x121Address",		0,	&DS_A_X121_ADDRESS },
/* 90 */ { "XFN",			0,	&DS_O_XFN },
/* 91 */ { "xFN",			0,	&DS_O_XFN },
/* 92 */ { "XFN-supplement",		0,	&DS_O_XFN_SUPPLEMENT },
/* 93 */ { "xFNSupplement",		0,	&DS_O_XFN_SUPPLEMENT }

};
const int	XDSInfo::string_to_oid_table_size =
		    sizeof (string_to_oid_table) / sizeof (string_to_oid_t);


const char	*XDSInfo::xds_problems[] = {
	"",
	"DS_E_ADMIN_LIMIT_EXCEEDED",
	"DS_E_AFFECTS_MULTIPLE_DSAS",
	"DS_E_ALIAS_DEREFERENCING_PROBLEM",
	"DS_E_ALIAS_PROBLEM",
	"DS_E_ATTRIBUTE_OR_VALUE_ALREADY_EXISTS",
	"DS_E_BAD_ARGUMENT",
	"DS_E_BAD_CLASS",
	"DS_E_BAD_CONTEXT",
	"DS_E_BAD_NAME",
	"DS_E_BAD_SESSION",
	"DS_E_BAD_WORKSPACE",
	"DS_E_BUSY",
	"DS_E_CANNOT_ABANDON",
	"DS_E_CHAINING_REQUIRED",
	"DS_E_COMMUNICATIONS_PROBLEM",
	"DS_E_CONSTRAINT_VIOLATION",
	"DS_E_DIT_ERROR",
	"DS_E_ENTRY_ALREADY_EXISTS",
	"DS_E_INAPPROP_AUTHENTICATION",
	"DS_E_INAPPROP_MATCHING",
	"DS_E_INSUFFICIENT_ACCESS_RIGHTS",
	"DS_E_INVALID_ATTRIBUTE_SYNTAX",
	"DS_E_INVALID_ATTRIBUTE_VALUE",
	"DS_E_INVALID_CREDENTIALS",
	"DS_E_INVALID_REF",
	"DS_E_INVALID_SIGNATURE",
	"DS_E_LOOP_DETECTED",
	"DS_E_MISCELLANEOUS",
	"DS_E_MISSING_TYPE",
	"DS_E_MIXED_SYNCHRONOUS",
	"DS_E_NAMING_VIOLATION",
	"DS_E_NO_INFORMATION",
	"DS_E_NO_SUCH_ATTRIBUTE_OR_VALUE",
	"DS_E_NO_SUCH_OBJECT",
	"DS_E_NO_SUCH_OPERATION",
	"DS_E_NOT_ALLOWED_ON_NON_LEAF",
	"DS_E_NOT_ALLOWED_ON_RDN",
	"DS_E_NOT_SUPPORTED",
	"DS_E_OBJECT_CLASS_MOD_PROHIB",
	"DS_E_OBJECT_CLASS_VIOLATION",
	"DS_E_OUT_OF_SCOPE",
	"DS_E_PROTECTION_REQUIRED",
	"DS_E_TIME_LIMIT_EXCEEDED",
	"DS_E_TOO_LATE",
	"DS_E_TOO_MANY_OPERATIONS",
	"DS_E_TOO_MANY_SESSIONS",
	"DS_E_UNABLE_TO_PROCEED",
	"DS_E_UNAVAILABLE",
	"DS_E_UNAVAILABLE_CRIT_EXT",
	"DS_E_UNDEFINED_ATTRIBUTE_TYPE",
	"DS_E_UNWILLING_TO_PERFORM"
};


/*
 * Compare 2 object identifiers (in ASN.1 BER format)
 * (returns less-than, greater-than or equal)
 */
static int
compare_om_oids2(
	const void	*oid1,
	const void	*oid2
)
{
	return (memcmp(((oid_to_string_t *)oid1)->oid->elements,
	    ((oid_to_string_t *)oid2)->oid->elements,
	    (size_t)((oid_to_string_t *)oid2)->oid->length));
}


/*
 * Compare 2 strings
 * (returns less-than, greater-than or equal)
 */
static int
compare_strings(
	const void	*str1,
	const void	*str2
)
{
	return (strcasecmp(((string_to_oid_t *)str1)->string,
	    ((string_to_oid_t *)str2)->string));
}


/*
 * Fill an OM_descriptor structure (no argument validation)
 */
void
XDSInfo::fill_om_desc(
	OM_descriptor	&desc
) const
{
	desc.type = OM_NO_MORE_TYPES;
	desc.syntax = OM_S_NO_MORE_SYNTAXES;
	desc.value.string.length = 0;
	desc.value.string.elements = OM_ELEMENTS_UNSPECIFIED;
}


/*
 * Fill an OM_descriptor structure (no argument validation)
 */
void
XDSInfo::fill_om_desc(
	OM_descriptor	&desc,
	const OM_type	type,
	OM_descriptor	*object
) const
{
	desc.type = type;
	desc.syntax = OM_S_OBJECT;
	desc.value.object.padding = 0;
	desc.value.object.object = object;
}


/*
 * Fill an OM_descriptor structure (no argument validation)
 */
void
XDSInfo::fill_om_desc(
	OM_descriptor	&desc,
	const OM_type	type,
	const OM_string	oid
) const
{
	desc.type = type;
	desc.syntax = OM_S_OBJECT_IDENTIFIER_STRING;
	desc.value.string.length = oid.length;
	desc.value.string.elements = oid.elements;
}


/*
 * Fill an OM_descriptor structure (no argument validation)
 */
void
XDSInfo::fill_om_desc(
	OM_descriptor		&desc,
	const OM_type		type,
	const OM_syntax		syntax,
	void			*string,
	const OM_string_length	length
) const
{
	desc.type = type;
	desc.syntax = syntax;
	desc.value.string.length = length;
	desc.value.string.elements = string;

	// explicitly set the length (XOM has problems)
	if (length == OM_LENGTH_UNSPECIFIED)
		desc.value.string.length = strlen((const char *)string);
}


/*
 * Fill an OM_descriptor structure (no argument validation)
 */
void
XDSInfo::fill_om_desc(
	OM_descriptor	&desc,
	const OM_type	type,
	const OM_sint32	number,
	const OM_syntax	syntax
) const
{
	desc.type = type;
	if ((desc.syntax = syntax) == OM_S_ENUMERATION)
		desc.value.enumeration = number;
	else
		desc.value.integer = number;
}


/*
 * Fill an OM_descriptor structure (no argument validation)
 */
void
XDSInfo::fill_om_desc(
	OM_descriptor		&desc,
	const OM_type		type,
	const OM_boolean	boolean
) const
{
	desc.type = type;
	desc.syntax = OM_S_BOOLEAN;
	desc.value.boolean = boolean;
}


/*
 * Extract the innermost component(s) from a multi-level XOM object.
 * The path through the object is specified by the 'route' argument.
 */
OM_return_code
XDSInfo::deep_om_get(
	OM_type_list		route,
	const OM_private_object	original,
	const OM_exclusions	exclusions,
	const OM_type_list	included_types,
	const OM_boolean	local_strings,
	const OM_value_position	initial_value,
	const OM_value_position	limiting_value,
	OM_public_object	*copy,
	OM_value_position	*total_number
) const
{
	OM_type			tl[] = {0, 0};
	OM_return_code		rc;
	OM_private_object	obj_in = original;
	OM_exclusions		excl = OM_EXCLUDE_ALL_BUT_THESE_TYPES +
				    OM_EXCLUDE_SUBOBJECTS;
	OM_public_object	obj_out;
	OM_value_position	num;

	while (tl[0] = *route++) {

		rc = om_get(obj_in, excl, tl, OM_FALSE, 0, 0, &obj_out, &num);

		if (rc != OM_SUCCESS) {
			om_delete(obj_out);
			return (rc);
		}

		if (num)
			obj_in = obj_out->value.object.object;

		om_delete(obj_out);
	}
	return (om_get(obj_in, exclusions, included_types, local_strings,
	    initial_value, limiting_value, copy, total_number));
}


/*
 * Map an attribute object identifier in ASN.1 BER format to its OM syntax
 * (and OM class).
 */
int
XDSInfo::om_oid_to_syntax(
	OM_object_identifier	*oid,
	OM_syntax		*syntax,
	OM_object_identifier	**class_oid
) const
{
	oid_to_string_t	key;
	oid_to_string_t	*entry;

	key.oid = oid;
	entry = (oid_to_string_t *)bsearch(&key, oid_to_string_table,
	    oid_to_string_table_size, sizeof (oid_to_string_t),
	    compare_om_oids2);

	if (entry) {
		*syntax = entry->syntax;
		if (class_oid)
			*class_oid = entry->class_oid;
		return (1);
	}
	else
		return (0);
}


/*
 * Map an object identifier in ASN.1 BER format to a string.
 */
unsigned char *
XDSInfo::om_oid_to_string(
	OM_object_identifier	*oid
) const
{
	oid_to_string_t	key;
	oid_to_string_t	*entry;
	unsigned char	*cp2;

	key.oid = oid;
	entry = (oid_to_string_t *)bsearch(&key, oid_to_string_table,
	    oid_to_string_table_size, sizeof (oid_to_string_t),
	    compare_om_oids2);

	if (entry) {
		char	*cp = string_to_oid_table[entry->index].string;
		int	len = strlen(cp) + 1;	// include terminator

		if (! (cp2 = new unsigned char [len]))
			return (0);

		return ((unsigned char *)memcpy(cp2, cp, (size_t)len));
	}
	else
		return (0);
}


/*
 * Map an object identifier string to ASN.1 BER format.
 */
OM_object_identifier *
XDSInfo::string_to_om_oid(
	unsigned char	*oid
) const
{
	string_to_oid_t	key;
	string_to_oid_t	*entry;

	key.string = (char *)oid;
	entry = (string_to_oid_t *)bsearch(&key, string_to_oid_table,
	    string_to_oid_table_size, sizeof (string_to_oid_t),
	    compare_strings);

	if (entry) {
		OM_string	*om_oid = new OM_string;
		int		len = (int)entry->oid->length;

		om_oid->length = len;
		if (! (om_oid->elements = new unsigned char [len]))
			return (0);

		memcpy(om_oid->elements, entry->oid->elements, len);
		return (om_oid);
	}
	else
		return (0);
}


/*
 * Convert a numeric XDS problem code into a string form
 */
unsigned char *
XDSInfo::xds_problem_to_string(
	int	p
) const
{
	return ((unsigned char *)xds_problems[p]);
}


/*
 * Compare 2 object identifiers (in ASN.1 BER format)
 * (returns equal or not-equal)
 */
int
XDSInfo::compare_om_oids(
	OM_object_identifier	&oid1,
	OM_object_identifier	&oid2
) const
{
	int	i;

	if ((i = (int)oid1.length) == oid2.length) {
		// test final octets first
		while ((--i >= 0) && (((unsigned char *)oid1.elements)[i] ==
		    ((unsigned char *)oid2.elements)[i])) {
		}
		if (i < 0)
			return (1);
	}

	return (0);
}


/*
 * Map an object identifier in ASN.1 BER format into its dotted string format
 * (e.g. ASN.1 BER decoding of "\x55\x04\x06" produces "2.5.4.6")
 */
unsigned char *
XDSInfo::om_oid_to_string_oid(
	OM_object_identifier	*oid
) const
{
	unsigned char	buf[max_oid_length];
	unsigned char	idbuf[5];
	unsigned char	*bp = buf;
	unsigned char	*cp;
	unsigned char	*cp2;
	unsigned int	id;
	int		i = 0;
	int		j;
	int		k;

	cp = (unsigned char *)oid->elements;

	// ASN.1 BER: first x 40 + second
	bp += sprintf((char *)bp, "%d", *cp / 40);
	*bp++ = '.';
	bp += sprintf((char *)bp, "%d", *cp % 40);
	cp++;

	for (i = 1; i < oid->length; i++, cp++) {
		if (*cp <= 0x7F)
			bp += sprintf((char *)bp, ".%d", *cp);
		else {
			// ASN.1 BER: skip high bit and extract block of 7 bits
			id = idbuf[0] = 0;
			for (j = 1; *cp > 0x7F; j++, i++, cp++)  {
				idbuf[j] = *cp & 0x7F;
			}
			idbuf[j] = *cp;
			for (k = 0; j; k++, j--) {
				id |= (idbuf[j] >> k |
				    (idbuf[j-1] << (7 - k) & 0xFF)) << 8 * k;
			}
			bp += sprintf((char *)bp, ".%d", id);
		}
	}
	*bp++ = '\0';
	int	len = bp - buf;

	if (! (cp2 = new unsigned char [len])) {
		return (0);
	}

	return ((unsigned char *)memcpy(cp2, buf, (size_t)len));
}


/*
 * Map an object identifier in dotted string format into its ASN.1 BER format
 * (e.g. ASN.1 BER encoding of "2.5.4.6" produces "\x55\x04\x06")
 */
OM_object_identifier *
XDSInfo::string_oid_to_om_oid(
	const char	*oid
) const
{
	unsigned char	oid_buf[max_oid_length];	// BER encoded object id
	unsigned char	sub_buf[5];	// BER encoded sub-identifier
	unsigned char	*oid_copy;	// string object identifier (copy)
	unsigned char	*cp1;
	unsigned char	*cp2;
	unsigned int	id;		// decimal identifier
	int		i = 0;
	int		j;

	// naive test
	if (((*oid != '0') && (*oid != '1') && (*oid != '2')) ||
	    (! strchr(oid, '.'))) {
		return (0);
	}

	// make a copy of the supplied oid string (the copy will be modified)
	int	len = strlen(oid);

	if (! (cp1 = new unsigned char [len + 1]))
		return (0);

	oid_copy = (unsigned char *)memcpy(cp1, oid, (size_t)len + 1);

	while (cp1) {
		// locate the dot that separates identifiers
		if (cp2 = (unsigned char *)strchr((const char *)cp1, '.'))
			*cp2 = '\0';	// mark end of string
		id = atol((const char *)cp1);
		if (id <= 0x7F) {
			oid_buf[i++] = (unsigned char)id;
		} else {
			// ASN.1 BER: set high bit and fill block of 7 bits
			for (j = 0; id; id >>= 7, j++) {
				sub_buf[j] = (unsigned char)id & 0x7F |
				    (j ? 0x80 : 0);
			}
			while (j)
				oid_buf[i++] = sub_buf[--j];
		}
		if (cp2)
			cp1 = ++cp2;
		else
			cp1 = cp2;
	}
	delete [] oid_copy;

	// ASN.1 BER: first x 40 + second
	oid_buf[1] = (oid_buf[0] * 40) + oid_buf[1];

	OM_string	*om_oid = new OM_string;

	om_oid->length = i - 1;	// the first entry will be skipped
	om_oid->elements = memcpy(new unsigned char [i - 1], &oid_buf[1],
	    (size_t)i - 1);
	return (om_oid);
}


/*
 * Map an object identifier's string abbreviation to its ASN.1 BER format.
 * Also accepts dotted decimal object identifiers (e.g. 2.5.4.0).
 * Set the appropriate syntax.
 *
 * e.g.  'C' or 'c'  -> DS_A_COUNTRY_NAME
 *      'CN' or 'cn' -> DS_A_COMMON_NAME
 *       'O' or 'o'  -> DS_A_ORG_NAME
 *	'OU' or 'ou' -> DS_A_ORG_UNIT_NAME
 *       'L' or 'l'  -> DS_A_LOCALITY_NAME
 *      'ST' or 'st' -> DS_A_STATE_OR_PROV_NAME
 *      'SN' or 'sn' -> DS_A_SURNAME
 *
 * %%% use XDS init files to locate valid abbreviations?
 */
OM_object_identifier *
XDSInfo::abbrev_to_om_oid(
	const char	*oid,
	OM_syntax	&syntax
) const
{
	OM_object_identifier	*om_oid;

	syntax = OM_S_TELETEX_STRING;	// default

	if (strcasecmp(oid, "C") == 0) {
		syntax = OM_S_PRINTABLE_STRING;
		om_oid = &DS_A_COUNTRY_NAME;

	} else if (strcasecmp(oid, "CN") == 0) {
		om_oid = &DS_A_COMMON_NAME;

	} else if (strcasecmp(oid, "O") == 0) {
		om_oid = &DS_A_ORG_NAME;

	} else if (strcasecmp(oid, "OU") == 0) {
		om_oid = &DS_A_ORG_UNIT_NAME;

	} else if (strcasecmp(oid, "L") == 0) {
		om_oid = &DS_A_LOCALITY_NAME;

	} else if (strcasecmp(oid, "ST") == 0) {
		om_oid = &DS_A_STATE_OR_PROV_NAME;

	} else if (strcasecmp(oid, "SN") == 0) {
		om_oid = &DS_A_SURNAME;

	} else if (strchr(oid, '.')) {
		return (string_oid_to_om_oid(oid));

	} else
		return (0);

	OM_object_identifier	*om_oid2 = new OM_object_identifier;
	int			len = (int) om_oid->length;

	om_oid2->length = len;
	if (! (om_oid2->elements = new unsigned char [len]))
		return (0);
	memcpy(om_oid2->elements, om_oid->elements, (size_t)len);

	return (om_oid2);
}


/*
 * Map an object identifier's ASN.1 BER format to its string abbreviation.
 * Also produces dotted decimal object identifiers (e.g. 2.5.4.0).
 *
 * e.g. DS_A_COUNTRY_NAME       -> 'C'
 *      DS_A_COMMON_NAME        -> 'CN'
 *      DS_A_ORG_NAME           -> 'O'
 *      DS_A_ORG_UNIT_NAME      -> 'OU'
 *      DS_A_LOCALITY_NAME      -> 'L'
 *      DS_A_STATE_OR_PROV_NAME -> 'ST'
 *      DS_A_SURNAME            -> 'SN'
 *
 * %%% use XDS init files to locate valid abbreviations?
 */
unsigned char *
XDSInfo::om_oid_to_abbrev(
	OM_object_identifier	&om_oid,
	unsigned char		*oid
) const
{
	if (compare_om_oids(DS_A_COUNTRY_NAME, om_oid))
		return ((unsigned char *)strcat((char *)oid, "C=") + 2);

	if (compare_om_oids(DS_A_COMMON_NAME, om_oid))
		return ((unsigned char *)strcat((char *)oid, "CN=") + 3);

	if (compare_om_oids(DS_A_ORG_NAME, om_oid))
		return ((unsigned char *)strcat((char *)oid, "O=") + 2);

	if (compare_om_oids(DS_A_ORG_UNIT_NAME, om_oid))
		return ((unsigned char *)strcat((char *)oid, "OU=") + 3);

	if (compare_om_oids(DS_A_LOCALITY_NAME, om_oid))
		return ((unsigned char *)strcat((char *)oid, "L=") + 2);

	if (compare_om_oids(DS_A_STATE_OR_PROV_NAME, om_oid))
		return ((unsigned char *)strcat((char *)oid, "ST=") + 3);

	if (compare_om_oids(DS_A_SURNAME, om_oid))
		return ((unsigned char *)strcat((char *)oid, "SN=") + 3);

	// other object identifiers: convert to dotted decimal
	unsigned char	*cp = om_oid_to_string_oid(&om_oid);

	oid = (unsigned char *)strcat((char *)oid, (char *)cp) +
	    strlen((char *)cp);
	*oid++ = '=';
	delete [] cp;

	return (oid);
}
