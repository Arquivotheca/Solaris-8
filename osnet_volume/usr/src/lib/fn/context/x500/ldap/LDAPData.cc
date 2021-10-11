/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)LDAPData.cc	1.7	96/04/17 SMI"

#include <string.h>
#include <ctype.h>	// isspace()
#include "LDAPData.hh"


/*
 * LDAP data structure manipulation
 */


/*
 * Convert a distinguished name (in the string format specified in
 * X/Open DCE Directory) into the string format specified in RFC 1779.
 *
 * %%% accept RDN as input too
 */
unsigned char *
LDAPData::string_to_ldap_dn(
	const FN_string	&dn
)
{
	const unsigned char	*src;
	const unsigned char	*s1;
	const unsigned char	*s2;
	unsigned char		*dst;
	unsigned char		*d1;
	unsigned char		*d2;
	int			len = dn.bytecount();

	s1 = s2 = src = dn.str();

	if (*s1 == '/') { 	// skip over leading slash, if present
		s2++;
		len--;
	}
	dst = new unsigned char [len + 2];
	d2 = &dst[len + 1];

	while (*s2) {

		s2 = ++s1;

		// scan RDN
		while (*s2 && (*s2 != '/'))
			s2++;

		// %%% translate OIDs e.g. "2.5.4.0" -> "OID.2.5.4.0"

		d1 = d2 - (s2 - s1) - 1;
		d2 = d1;	// reset

		if (s1 != s2) {	// copy RDN
			while (s1 != s2) {
				// translate multiple RDN separator
				if (*s1 == ',')
					*d1 = '+';
				else
					*d1 = *s1;
				d1++;
				s1++;
			}
			*d1 = ',';	// add RDN separator
		} else {	// RDN is empty
			*d1 = ' ';
		}
	}
	dst[len] = '\0';

	x500_trace("LDAPData::string_to_ldap_dn: \"%s\"\n", dst);
	return (dst);
}


/*
 * Convert a distinguished name (in the string format specified in
 * RFC 1779) into the string format specified in X/Open DCE Directory.
 */
unsigned char *
LDAPData::ldap_dn_to_string(
	char	*name,
	int	is_rdn,
	int	&err
)
{
	const unsigned char	*np = (unsigned char *)name;
	int			len = strlen(name);
	unsigned char		*rdn;
	unsigned char		*rp;
	unsigned char		*dn;
	unsigned char		*dp;
	int			offset;

	x500_trace("LDAPData::ldap_dn_to_string: \"%s\"\n", name);

	if (! (rp = rdn = new unsigned char [len + 2])) {
		err = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	if (! (dn = new unsigned char [len + 2])) {
		err = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	dn[len + 1] = '\0';
	dp = &dn[len + 1];

	while (*np) {

		*rp++ = '/';

		// skip any leading spaces
		while (*np && (isspace(*np)))
			np++;

		while (*np) {

			if (*np == '"') {

				// handle quotes
				while (*np && (*np != '"'))
					*rp++ = *np++;
				if (*np == '"')
					*rp++ = *np++;

			} else if (((*np == ';') || (*np == ',')) &&
			    (*(np - 1) != '\\')) {

				// handle RDN separator
				np++;
				break;


			} else if ((isspace(*np)) && (*(np + 1) == '=') &&
			    (isspace(*(np + 2)))) {

				// handle ' = '
				np = np + 3;
				*rp++ = '=';

			} else if ((isspace(*np)) && (isspace(*(np + 1)))) {

				// handle multiple spaces
				np++;

			} else if (((*np == 'O') || (*np == 'o')) &&
			    ((*(np + 1) == 'I') || (*(np + 1) == 'i')) &&
			    ((*(np + 2) == 'D') || (*(np + 2) == 'd')) &&
			    (*(np + 3) == '.')) {

				// handle OIDs
				np = np + 4;

			} else if ((*np == '+') && (*(np - 1) != '\\')) {

				// multiple RDNs
				if (isspace(*(rp - 1)))
					rp--;
				*rp++ = ',';
				np++;

			} else {

				*rp++ = *np++;

			}
		}
		// skip any trailing spaces
		while (isspace(*(rp - 1)))
			rp--;

		*rp = '\0';

		// copy RDN
		dp = dp - (rp - rdn);
		memcpy(dp, rdn, (rp - rdn));

		rp = rdn; // reset
	}
	delete [] rdn;

	if (is_rdn)
		dp++;	// omit initial slash

	offset = dp - dn;
	while (*dp) {
		*(dp - offset) = *dp;
		dp++;
	}
	*(dp - offset) = '\0';

	err = FN_SUCCESS;
	return (dn);
}

/*
 * Return position of next RDN in a distinguished name
 */
unsigned char *
LDAPData::get_next_ldap_rdn(
	char	*dn
)
{
	unsigned char	*np = (unsigned char *)dn;

	// skip any leading spaces
	while (np && (isspace(*np)))
		np++;

	while (*np) {

		// skip any leading spaces
		while (*np && (isspace(*np)))
			np++;

		if (*np == '"') {

			// handle quotes
			while (*np && (*np != '"'))
				np++;
			if (! *np)
				return (0);

		} else if (((*np == ';') || (*np == ',')) &&
		    (*(np - 1) != '\\')) {

			// handle RDN separator
			np++;
			break;

		}
		np++;
	}
	// skip any leading spaces
	while (*np && (isspace(*np)))
		np++;

	return (np);
}


/*
 * Examine 2 LDAP distinguished names (from right-to-left).
 * Exclude initial matching RDNs from the second name.
 */
int
LDAPData::exclude_matching_rdns(
	char	*parent,
	char	*child
)
{
	int	ip = strlen(parent);
	int	ic = strlen(child);

	// if parent is ROOT then nothing to exclude
	if (ip == 0)
		return (1);	// match

	if (ic == 0)
		return (0);	// no match

	ip--;
	ic--;
	while ((ip >= 0) && (ic >= 0)) {

		if (isspace(parent[ip])) {
			ip--;
			continue;
		}

		if (isspace(child[ic])) {
			ic--;
			continue;
		}

		if (((parent[ip] == ';') || (parent[ip] == ',')) &&
		    ((child[ic] == ';') || (child[ic] == ','))) {
			ip--;
			ic--;
			continue;
		}

		if (tolower(parent[ip]) == tolower(child[ic])) {
			ip--;
			ic--;
		} else {
			break;
		}
	}
	if (ip < 0) {	// child extends parent, exclude parent's RDNs.

		while (isspace(child[ic]))
			ic--;

		if ((child[ic] == ';') || (child[ic] == ',')) {
			child[ic] = '\0';
			return (1);	// match
		} else {
			return (0);	// no match
		}
	}

	return (0);	// no match
}


/*
 * Map LDAP error code to XFN error code
 */
int
LDAPData::ldap_error_to_xfn(
	int	&ldap_err
)
{
	switch (ldap_err) {
	case LDAP_NO_SUCH_OBJECT:
		return (FN_E_NAME_NOT_FOUND);

	case LDAP_ALREADY_EXISTS:
		return (FN_E_NAME_IN_USE);

	case LDAP_NO_SUCH_ATTRIBUTE:
		return (FN_E_NO_SUCH_ATTRIBUTE);

	case LDAP_INVALID_SYNTAX:
	case LDAP_CONSTRAINT_VIOLATION:
		return (FN_E_INVALID_ATTR_VALUE);

	case LDAP_UNDEFINED_TYPE:
		return (FN_E_INVALID_ATTR_IDENTIFIER);

	case LDAP_INSUFFICIENT_ACCESS:
		return (FN_E_CTX_NO_PERMISSION);

	case LDAP_INVALID_CREDENTIALS:
	case LDAP_INAPPROPRIATE_AUTH:
	case LDAP_AUTH_UNKNOWN:
	case LDAP_STRONG_AUTH_NOT_SUPPORTED:
	case LDAP_STRONG_AUTH_REQUIRED:
		return (FN_E_AUTHENTICATION_FAILURE);

	case LDAP_INVALID_DN_SYNTAX:
	case LDAP_NAMING_VIOLATION:
		return (FN_E_ILLEGAL_NAME);

	case LDAP_UNAVAILABLE:
	case LDAP_BUSY:
		return (FN_E_CTX_UNAVAILABLE);

	case LDAP_SERVER_DOWN:
		return (FN_E_COMMUNICATION_FAILURE);

	case LDAP_UNWILLING_TO_PERFORM:
	case LDAP_NOT_ALLOWED_ON_RDN:
	case LDAP_NOT_ALLOWED_ON_NONLEAF:
	case LDAP_NO_OBJECT_CLASS_MODS:
		return (FN_E_OPERATION_NOT_SUPPORTED);

	// %%% others

	default:
		return (FN_E_UNSPECIFIED_ERROR);
	}
}


/*
 * Convert an LDAP entry to an XFN reference
 *
 * NOTE: additional attributes are fetched when necessary
 */
FN_ref *
LDAPData::ldap_entry_to_ref(
	LDAP		*ld,
	unsigned char	*name,
	LDAPMessage	*entry,
	int		&err
)
{
	FN_ref			*ref = 0;
	FN_ref_addr		x500_ref_addr(x500, strlen((char *)name), name);
	FN_identifier		*id = 0;
	FN_attrset		*attrs = 0;
	const FN_identifier	*rid = 0;
	char			**values;

	// use objectReferenceString attribute (if present) to build reference
	if (values = ldap_get_values(ld, entry, "objectReferenceString")) {

		if (! (ref = string_ref_to_ref((unsigned char *)values[0],
		    strlen(values[0]), &attrs, err))) {
			ldap_value_free(values);
			return (0);
		}
		ldap_value_free(values);
		rid = ref->type();

	} else {

		// %%% handle objectReferenceId and objectReferenceAddresses
		if (values = ldap_get_values(ld, entry, "objectReferenceId")) {
			ldap_value_free(values);
			err = FN_E_OPERATION_NOT_SUPPORTED;
			return (0);
		}
	}

	if (! ref) {

		// use objectClass attribute (if present) to set reference type
		if (values = ldap_get_values(ld, entry, "objectClass")) {
			id = ldap_attr_to_ref_type(values);
			ldap_value_free(values);
		}
		if (! id)
			id = new FN_identifier((const unsigned char *)"x500");

		if ((ref = new FN_ref(*id)) == 0) {
			delete id;
			err = FN_E_INSUFFICIENT_RESOURCES;
			return (0);
		}
	} else {
		if (attrs) { // retrieve additional attributes

			x500_trace("LDAPData::ldap_entry_to_ref: %s\n",
			    "additional attributes required");

		// %%% ldap_entry_to_ref(): retrieve additional attributes
		}
	}
	ref->append_addr(x500_ref_addr);

	// append presentationAddress attribute (if present) to reference
	if (values = ldap_get_values(ld, entry, "presentationAddress")) {
		ref->append_addr(FN_ref_addr(paddr, strlen(values[0]),
		    values[0]));
		ldap_value_free(values);
	}

	x500_trace("LDAPData::ldap_entry_to_ref: created %s (%s) for %s\n",
	    "XFN reference", (rid ? rid->str() : id->str()), name);

	delete id;
	return (ref);
}


/*
 * Map a string object class onto an XFN reference type
 *
 * organization           -> onc_fn_enterprise
 * organizationalUnit     -> onc_fn_organization
 * person                 -> onc_fn_user
 * locality               -> onc_fn_site
 * applicationProcess     -> onc_fn_service
 *
 * all others are mapped to the reference type: x500
 */

FN_identifier *
LDAPData::ldap_attr_to_ref_type(
	char	**values
) const
{
	int	i;

	for (i = 0; values[i] != NULL; i++) {

		if (strcmp("top", values[i]) == 0)
			continue;

		if ((strcmp("alias", values[i]) == 0) ||
		    (strcmp("groupOfNames", values[i]) == 0) ||
		    (strcmp("country", values[i]) == 0)) {
			break;
		}

		// MHS object classes
		if (strncmp("mhs", values[i], 3) == 0)
			break;

		if (strcmp("person", values[i]) == 0)
			return (new FN_identifier(
			    (const unsigned char *)"onc_fn_user"));

		if (strcmp("organization", values[i]) == 0)
			return (new FN_identifier(
			    (const unsigned char *)"onc_fn_enterprise"));

		if (strcmp("organizationalUnit", values[i]) == 0)
			return (new FN_identifier(
			    (const unsigned char *)"onc_fn_organization"));

		if (strcmp("locality", values[i]) == 0)
			return (new FN_identifier(
			    (const unsigned char *)"onc_fn_site"));

		if (strcmp("applicationProcess", values[i]) == 0)
			return (new FN_identifier(
			    (const unsigned char *)"onc_fn_service"));

	}

	return (new FN_identifier((const unsigned char *)"x500"));
}


/*
 * Convert an XFN identifier to an LDAP attribute identifier
 */
const unsigned char *
LDAPData::id_to_ldap_attr(
	const FN_identifier	&id,
	int			&err
)
{
	const unsigned char	*attr;

	switch (id.format()) {

	case FN_ID_STRING:
	case FN_ID_ISO_OID_STRING:
		if (! (attr = id.str())) {
			err = FN_E_INSUFFICIENT_RESOURCES;
			return (0);
		}
		break;

	case FN_ID_DCE_UUID:
		// fall through

	default:
		x500_trace("LDAPData::id_to_ldap_attr: %s\n",
		    "unknown identifier format");
		err = FN_E_INVALID_ATTR_IDENTIFIER;
		return (0);
	}
	x500_trace("LDAPData::id_to_ldap_attr: \"%s\" attribute\n", attr);

	return (attr);
}


/*
 * Convert an XFN attribute set to an array of LDAP attribute identifiers
 * (append additional attribute IDs, if present)
 */
const unsigned char **
LDAPData::ids_to_ldap_attrs(
	const FN_attrset	*ids,
	char			**more_attrs,
	int			&err
)
{
	const unsigned char	**attrs;
	const FN_attribute	*attr;
	const FN_identifier	*id;
	void			*iter;
	int			id_num = ids->count();
	int			more = 0;
	int			i = 0;

	// count additional attribute IDs
	if (more_attrs) {
		while (more_attrs[more]) {
			more++;
		}
	}

	if (! (attrs = new const unsigned char * [id_num + more])) {
		err = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}

	for (i = 0, attr = ids->first(iter);
	    attr;
	    attr = ids->next(iter), i++) {

		if ((! (id = attr->identifier())) ||
		    (! (attrs[i] = id_to_ldap_attr(*id, err)))) {
			x500_trace("LDAPData::ids_to_ldap_attrs: error\n");
			delete [] attrs;
			err = FN_E_INVALID_ATTR_IDENTIFIER;
			return (0);
		}
	}

	// append additional attribute IDs
	more = 0;
	if (more_attrs) {
		while (more_attrs[more]) {
			attrs[i++] = (unsigned char *)more_attrs[more++];
		}
	}
	attrs[i] = 0;

	x500_trace("LDAPData::ids_to_ldap_attrs: %d+%d attribute IDs\n", id_num,
	    more);

	return (attrs);
}


/*
 * locate the specified attribute value in an array of attribute values
 */
int
LDAPData::locate_ldap_value(
	char		**values,
	struct berval	*bval
)
{
	int i = 0;

	if (! values)
		return (0);

	while (values[i]) {
		if ((strlen(values[i]) == bval->bv_len) &&
		    ((memcmp(values[i], bval->bv_val, (size_t)bval->bv_len)) ==
		    0)) {
			break;
		}
		i++;
	}

	return (values[i] ? 1 : 0);
}


/*
 * Free the storage allocated for LDAP modifications
 */
void
LDAPData::cleanup_ldap_mods(
	LDAPMod	**mod_list
)
{
	if (mod_list) {
		if (*mod_list) {
			if ((*mod_list)->mod_bvalues) {
				if (*(*mod_list)->mod_bvalues) {
					delete [] *(*mod_list)->mod_bvalues;
				}
				delete [] (*mod_list)->mod_bvalues;
			}
			delete [] *mod_list;
		}
		delete [] mod_list;
	}
}


/*
 * Convert XFN modify operation to equivalent LDAP modify operation
 */
int
LDAPData::mod_op_to_ldap_mod_op(
	unsigned int	mod_op,
	int		&ldap_mod_op,
	char		**values,
	int		&err
)
{
	switch (mod_op) {
	case FN_ATTR_OP_ADD_EXCLUSIVE:
		// if attribute already exists in X.500:
		//	error
		// if attribute does not exist in X.500:
		//	XFN add-exclusive -> LDAP add

		if (values) {
			err = FN_E_ATTR_IN_USE;
			return (-1);
		} else {
			ldap_mod_op = LDAP_MOD_ADD;
			x500_trace("LDAPData::mod_op_to_ldap_mod_op: %s\n",
			    "add operation");
		}
		break;

	case FN_ATTR_OP_ADD:
		// XFN add -> LDAP replace

		ldap_mod_op = LDAP_MOD_REPLACE;
		x500_trace("LDAPData::mod_op_to_ldap_mod_op: %s\n",
		    "replace operation");
		break;

	case FN_ATTR_OP_ADD_VALUES:
		// XFN add-values -> LDAP add
		// (only add those values which do not exist)

		ldap_mod_op = LDAP_MOD_ADD;
		x500_trace("LDAPData::mod_op_to_ldap_mod_op: %s\n",
		    "add operation");
		break;

	case FN_ATTR_OP_REMOVE:
		// if attribute already exists in X.500:
		//	XFN remove -> LDAP delete
		//	(ignore any supplied attribute values)
		// if attribute does not exist in X.500:
		//	skip this modification

		if (values) {
			ldap_mod_op = LDAP_MOD_DELETE;
			x500_trace("LDAPData::mod_op_to_ldap_mod_op: %s\n",
			    "delete operation");
		} else {
			return (0);	// no modification necessary
		}
		break;

	case FN_ATTR_OP_REMOVE_VALUES:
		// if attribute already exists in X.500:
		//	XFN remove-values -> LDAP delete
		//	(only delete those values which already exist)
		// if attribute does not exist in X.500:
		//	skip this modification

		if (values) {
			ldap_mod_op = LDAP_MOD_DELETE;
			x500_trace("LDAPData::mod_op_to_ldap_mod_op: %s\n",
			    "delete operation");
		} else {
			return (0);	// no modification necessary
		}
		break;

	default:
		err = FN_E_OPERATION_NOT_SUPPORTED;
		return (-1);
	}

	return (1);
}


/*
 * Convert XFN modifications into an LDAP attribute modification list
 *
 * NOTE: The semantics of XFN and LDAP modify operations do not match
 *       exactly. The entry to be modified must be pre-read and the
 *       XFN modifications adjusted accordingly.
 */
LDAPMod **
LDAPData::mods_to_ldap_mods(
	const FN_attrmodlist	&mods,
	LDAP			*ld,
	LDAPMessage		*entry,
	int			&err
)
{
	int	mod_num;

	if ((mod_num = mods.count()) == 0) {
		err = FN_E_ATTR_VALUE_REQUIRED;
		return (0);
	}

	x500_trace("LDAPData::mods_to_ldap_mods: %d XFN modify operation%s\n",
	    mod_num, (mod_num == 1) ? "" : "s");

	unsigned int		mod_op;
	void			*mod_iter;
	const FN_attribute	*attr;
	const FN_identifier	*id;
	const unsigned char	*id_str;
	const FN_identifier	*syntax;
	const FN_attrvalue	*val;
	void			*val_iter;
	unsigned int		val_num;
	char			**values;
	LDAPMod			**mod_items;
	LDAPMod			*mod_item;
	struct berval		**mod_bvals;
	struct berval		*mod_bval;
	int			i;
	int			j;
	int			present;
	int			op_err;

	if ((mod_items = new LDAPMod * [mod_num + 1]) == 0) {
		err = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	if ((mod_item = new LDAPMod [mod_num]) == 0) {
		delete [] mod_items;
		err = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}

	// examine each XFN modification
	for (i = 0, attr = mods.first(mod_iter, mod_op);
	    attr != 0;
	    attr = mods.next(mod_iter, mod_op), i++) {

		// extract attribute identifier
		if (! (id = attr->identifier())) {
			x500_trace("LDAPData::mods_to_ldap_mods: ID error\n");
			delete [] mod_item;
			delete [] mod_items;
			err = FN_E_INVALID_ATTR_IDENTIFIER;
			return (0);
		} else
			id_str = id->str();

		// check if attribute already exists in the entry
		if (entry && (mod_op != FN_ATTR_OP_ADD)) {
			values = ldap_get_values(ld, entry, (char *)id_str);
		} else
			values = 0;

		// convert XFN modify operation to equivalent LDAP operation
		if ((op_err = mod_op_to_ldap_mod_op(mod_op, mod_item[i].mod_op,
		    values, err)) == -1) {
			delete [] mod_item;
			delete [] mod_items;
			if (values)
				ldap_value_free(values);
			return (0);
		} else {
			if (op_err == 0) {
				i--;	// roll back one
				continue;
			}
		}

		if (val_num = attr->valuecount()) {
			if (! (mod_bvals = new struct berval * [val_num + 1])) {
				delete [] mod_item;
				delete [] mod_items;
				if (values)
					ldap_value_free(values);
				err = FN_E_INSUFFICIENT_RESOURCES;
				return (0);
			}
			if (! (mod_bval = new struct berval [val_num])) {
				delete [] mod_bvals;
				delete [] mod_item;
				delete [] mod_items;
				if (values)
					ldap_value_free(values);
				err = FN_E_INSUFFICIENT_RESOURCES;
				return (0);
			}
			mod_item[i].mod_op |= LDAP_MOD_BVALUES;
			mod_item[i].mod_bvalues = mod_bvals;

		} else {
			mod_bval = 0;
			mod_bvals = 0;
			if ((mod_item[i].mod_op == LDAP_MOD_DELETE) &&
			    (mod_op == FN_ATTR_OP_REMOVE)) {
				mod_item[i].mod_bvalues = 0;
			} else {
				delete [] mod_item;
				delete [] mod_items;
				if (values)
					ldap_value_free(values);
				err = FN_E_INVALID_ATTR_VALUE;
				return (0);
			}
		}

		// extract attribute syntax
		if (! (syntax = attr->syntax())) {
			x500_trace("LDAPData::mods_to_ldap_mods: %s\n",
			    "syntax error");
			delete [] mod_bval;
			delete [] mod_bvals;
			delete [] mod_item;
			delete [] mod_items;
			if (values)
				ldap_value_free(values);
			err = FN_E_INVALID_SYNTAX_ATTRS;
			return (0);
		} else {
			if (! (*syntax == ascii)) {
				x500_trace("LDAPData::mods_to_ldap_mods: %s\n",
				    "non-ASCII value(s)");
				delete [] mod_bval;
				delete [] mod_bvals;
				delete [] mod_item;
				delete [] mod_items;
				if (values)
					ldap_value_free(values);
				err = FN_E_SYNTAX_NOT_SUPPORTED;
				return (0);
			}
		}
		x500_trace("LDAPData::mods_to_ldap_mods: \"%s\" attribute\n",
		    id_str);

		// handle attribute values
		for (j = 0, val = attr->first(val_iter);
		    val != 0;
		    val = attr->next(val_iter), j++) {

			mod_bval[j].bv_len = val->length();
			mod_bval[j].bv_val = (char *)val->contents();

			// %%% test against current mods too
			present = locate_ldap_value(values, &mod_bval[j]);

			// skip this value if it already exists in X.500
			if (present && (mod_op == FN_ATTR_OP_ADD_VALUES)) {
				j--;	// roll back one
				continue;
			}

			// skip this value if it already exists in X.500
			if ((! present) &&
			    (mod_op == FN_ATTR_OP_REMOVE_VALUES)) {
				j--;	// roll back one
				continue;
			}
			mod_bvals[j] = &mod_bval[j];

			x500_trace("LDAPData::%s value %d: \"%.*s\"\n",
			    "mods_to_ldap_mods:", j + 1, mod_bval[j].bv_len,
			    mod_bval[j].bv_val);
		}
		if (values)
			ldap_value_free(values);

		if ((j == 0) && (mod_op != FN_ATTR_OP_REMOVE)) {
			i--;	// roll back one
			continue;
		}
		if (mod_bvals)
			mod_bvals[j] = 0;
		mod_item[i].mod_type = (char *)id_str;
		mod_item[i].mod_next = 0;
		mod_items[i] = &mod_item[i];
	}
	if (i == 0) {
		delete [] mod_bval;
		delete [] mod_bvals;
		delete [] mod_item;
		delete [] mod_items;
		err = FN_SUCCESS;	// no modifications needed
		return (0);
	}
	mod_items[i] = 0;

	return (mod_items);
}


/*
 * Convert one or more attributes into an LDAP filter
 *
 *  LDAP filter:  "(&(attr-id=attr-value)(attr-id=attr-val) ...)"
 *
 */
unsigned char *
LDAPData::attrs_to_ldap_filter(
	const FN_attrset	*match_attrs,
	int			&err

)
{
	int		attr_num;

	if ((! match_attrs) || ((attr_num = match_attrs->count()) == 0)) {
		x500_trace("LDAPData::attrs_to_filter: empty filter\n");
		return ((unsigned char *)strcpy(new char[14], "objectClass=*"));
	}

	unsigned char		*filter;
	unsigned char		*fp;
	const FN_attribute	*attr;
	const FN_identifier	*id;
	const FN_attrvalue	*val;
	FN_string		*val_string;
	void			*attr_iter;
	void			*val_iter;
	const unsigned char	*id_str;
	const unsigned char	*val_str;
	int			id_len;
	int			val_len;
	int			i;

	x500_trace("LDAPData::attrs_to_filter: match %d attribute%s\n",
	    attr_num, (attr_num == 1) ? "" : "s");

	if (! (fp = filter = new unsigned char [max_filter_length])) {
		err = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}

	*fp++ = '(';
	*fp++ = '&';

	for (attr = match_attrs->first(attr_iter);
	    attr != NULL;
	    attr = match_attrs->next(attr_iter)) {

		// handle attribute ID

		if (! (id = attr->identifier())) {
			delete [] filter;
			err = FN_E_INSUFFICIENT_RESOURCES;
			return (0);
		}
		if (! (id_str = id_to_ldap_attr(*id, err))) {
			delete [] filter;
			return (0);
		}
		id_len = strlen((char *)id_str);

		// handle attribute values

		for (i = 0, val = attr->first(val_iter);
		    val != 0;
		    val = attr->next(val_iter), i++) {

			if ((val_string = val->string()) &&
			    (val_len = val_string->bytecount())) {

				// overflow?
				if (((fp - filter) + id_len + val_len + 5) >
				    max_filter_length) {
					delete val_string;
					delete [] filter;
					err = FN_E_SEARCH_INVALID_FILTER;
					x500_trace("LDAPData::%s\n",
					    "attrs_to_filter: filter overflow");
					return (0);
				}
				val_str = val_string->str();

				*fp++ = '(';
				memcpy(fp, id_str, id_len);
				fp += id_len;
				*fp++ = '=';
				memcpy(fp, val_str, val_len);
				fp += val_len;
				*fp++ = ')';

				delete val_string;
			} else {
				delete [] filter;
				err = FN_E_INVALID_ATTR_VALUE;
				return (0);
			}
		}

		// no values supplied
		if (i == 0) {
			// overflow?
			if ((id_len + (fp - filter) + 5) > max_filter_length) {
				delete [] filter;
				err = FN_E_SEARCH_INVALID_FILTER;
				x500_trace("LDAPData::attrs_to_filter: %s\n",
				    "filter overflow");
				return (0);
			}
			*fp++ = '(';
			memcpy(fp, id_str, id_len);
			fp += id_len;
			*fp++ = '=';
			*fp++ = '*';
			*fp++ = ')';
		}
	}

	*fp++ = ')';
	*fp++ = '\0';

	x500_trace("LDAPData::attrs_to_filter: \"%s\"\n", filter);
	return (filter);
}


/*
 * Extract components from XFN search control
 */
int
LDAPData::extract_search_controls(
	LDAP			*ld,
	const FN_search_control	*control,
	int			&scope,
	int			&follow_links,
	unsigned int		&return_ref,
	char			**&selection,
	char			*select_ref_attrs[],
	int			&err
)
{
	const FN_attrset	*ids;
	unsigned int		limit;

	if (control) {

		// scope of the search
		switch (control->scope()) {
		case FN_SEARCH_SUBTREE:
			scope = LDAP_SCOPE_SUBTREE;
			x500_trace("LDAPData::extract_search_controls: %s\n",
			    "scope: subtree");
			break;

		case FN_SEARCH_CONSTRAINED_SUBTREE:
			scope = LDAP_SCOPE_SUBTREE;
			x500_trace("LDAPData::extract_search_controls: %s\n",
			    "scope: constrained subtree");
			break;

		case FN_SEARCH_ONE_CONTEXT:
			scope = LDAP_SCOPE_ONELEVEL;
			x500_trace("LDAPData::extract_search_controls: %s\n",
			    "scope: one-level");
			break;

		case FN_SEARCH_NAMED_OBJECT:
			scope = LDAP_SCOPE_BASE;
			x500_trace("LDAPData::extract_search_controls: %s\n",
			    "scope: base");
			break;

		default:
			err = FN_E_SEARCH_INVALID_OPTION;
			return (0);
		}

		follow_links = control->follow_links();

		// set size limit for the search
		if (limit = control->max_names()) {
			ld->ld_sizelimit = limit;
			x500_trace("LDAPData::%s: %d\n",
			    "extract_search_controls: size limit", limit);
		} else {
			x500_trace("LDAPData::extract_search_controls: %s\n",
			    "no size limit");
		}

		return_ref = control->return_ref();
		if (ids = control->return_attr_ids()) {
			// convert attribute IDs to list of selected attributes
			// (append reference attributes, if necessary)
			if (! (selection = (char **)ids_to_ldap_attrs(ids,
			    return_ref ? select_ref_attrs : 0, err)))
				return (0);
		} else {
			selection = NULL;
			x500_trace("LDAPData::extract_search_controls: %s\n",
			    "requesting all attributes");
		}

	} else {
		scope = LDAP_SCOPE_ONELEVEL;
		follow_links = 0;
		selection = NULL;

		x500_trace("LDAPData::extract_search_controls: defaults\n");
	}

	return (1);
}


/*
 * Convert LDAP entries into an set of XFN names
 */
int
LDAPData::ldap_entries_to_set(
	LDAP		*ld,
	LDAPMessage	*entries,
	FN_nameset	*set
)
{
	return (ldap_entries_to_set2(ld, 0, entries, 0, XFN_NAME_SET, set, 0, 0,
	    0, 0));
}


/*
 * Convert LDAP entries into an set of XFN bindings
 */
int
LDAPData::ldap_entries_to_set(
	LDAP		*ld,
	LDAPMessage	*entries,
	FN_bindingset	*set
)
{
	return (ldap_entries_to_set2(ld, 0, entries, 1, XFN_BINDING_SET, 0, set,
	    0, 0, 0));
}


/*
 * Convert LDAP entries into an set of XFN attributes
 */
int
LDAPData::ldap_entries_to_set(
	LDAP		*ld,
	LDAPMessage	*entries,
	FN_attrset	*set
)
{
	return (ldap_entries_to_set2(ld, 0, entries, 0, XFN_ATTR_SET, 0, 0, set,
	    0, 0));
}


/*
 * Convert LDAP entries into an set of XFN entries
 */
int
LDAPData::ldap_entries_to_set(
	LDAP		*ld,
	LDAPMessage	*entries,
	unsigned int	return_ref,
	FN_searchset	*set
)
{
	return (ldap_entries_to_set2(ld, 0, entries, return_ref, XFN_SEARCH_SET,
	    0, 0, 0, set, 0));
}


/*
 * Convert LDAP entries into an set of XFN entries
 */
int
LDAPData::ldap_entries_to_set(
	LDAP			*ld,
	char			*base_dn,
	LDAPMessage		*entries,
	unsigned int		return_ref,
	FN_ext_searchset	*set
)
{
	return (ldap_entries_to_set2(ld, base_dn, entries, return_ref,
	    XFN_EXT_SEARCH_SET, 0, 0, 0, 0, set));
}


/*
 * Convert LDAP entries into the specified XFN set
 *
 *
 *                      | atomic full | reference | attributes
 *                      | name   name |           |
 *    ------------------|-------------|-----------|------------
 *     FN_nameset       |   X         |     -     |     -
 *     FN_bindingset    |   X         |     X     |     -
 *     FN_attrset       |   -         |     -     |     X
 *     FN_searchset     |   X         |     X     |     X
 *     FN_ext_searchset |   X     X   |     X     |     X
 *
 */
int
LDAPData::ldap_entries_to_set2(
	LDAP			*ld,
	char			*base_dn,
	LDAPMessage		*entries,
	unsigned int		return_ref,
	enum xfn_set_type	set_type,
	FN_nameset		*name_set,
	FN_bindingset		*binding_set,
	FN_attrset		*attr_set,
	FN_searchset		*search_set,
	FN_ext_searchset	*ext_search_set
)
{
	FN_ref		*ref = 0;
	FN_string	*dn_string;
	FN_string	*dots = 0;
	unsigned char	*xdn = 0;
	unsigned char	*xrdn = 0;
	LDAPMessage	*entry;
	char		*dn;
	char		**rdns;
	struct berval	**values;
	char		*at;
	BerElement	*ber;
	int		total = ldap_count_entries(ld, entries);
	int		err;
	int		i;
	int		a;
	unsigned int	status;

	x500_trace("LDAPData::ldap_entries_to_set: %d entr%s\n", total,
	    (total == 1) ? "y" : "ies");

	// build set

	if (set_type == XFN_EXT_SEARCH_SET) {
		if (! (dots = new FN_string((unsigned char *)"...")))
			return (FN_E_INSUFFICIENT_RESOURCES);
	}

	for (entry = ldap_first_entry(ld, entries);
	    entry != NULL;
	    entry = ldap_next_entry(ld, entry)) {

		// handle name

		if (set_type != XFN_ATTR_SET) {

			rdns = 0;
			xrdn = 0;
			dn = ldap_get_dn(ld, entry);

			if (set_type == XFN_EXT_SEARCH_SET) {

				if (exclude_matching_rdns(base_dn, dn)) {

					// use RDN(s) relative to current ctx
					// (entry is a child of current ctx)

					xdn = ldap_dn_to_string(dn, 1, err);
					if (err != FN_SUCCESS) {
						goto cleanup;
					}
					dn_string = new FN_string(xdn);

				} else {
					// use full DN relative to initial ctx
					// (entry is not a child of current ctx)

					xdn = ldap_dn_to_string(dn, 0, err);
					if (err != FN_SUCCESS) {
						goto cleanup;
					}

					FN_string	xdn_string(xdn);

					dn_string = new FN_string(&status, dots,
					    &xdn_string, 0);
				}

			} else {
				// use final RDN
				// (entry is a direct child of current ctx)
				rdns = ldap_explode_dn(dn, 0);

				xrdn = ldap_dn_to_string(rdns[0], 1, err);
				if (err != FN_SUCCESS) {
					goto cleanup;
				}
			}
		}

		// handle reference

		if (return_ref) {

			if (! xdn) {
				xdn = ldap_dn_to_string(dn, 0, err);
				if (err != FN_SUCCESS) {
					goto cleanup;
				}
			}

			// build reference
			if (! (ref = ldap_entry_to_ref(ld, xdn, entry, err))) {
				delete [] xdn;
				goto cleanup;
			}
		}

		// handle attributes

		if ((set_type != XFN_NAME_SET) &&
		    (set_type != XFN_BINDING_SET)) {

			if (((set_type == XFN_SEARCH_SET) ||
			    (set_type == XFN_EXT_SEARCH_SET)) &&
			    (! (attr_set = new FN_attrset))) {
				err = FN_E_INSUFFICIENT_RESOURCES;
				goto cleanup;
			}

			for (a = 0, at = ldap_first_attribute(ld, entry, &ber);
			    at != NULL;
			    at = ldap_next_attribute(ld, entry, ber), a++) {

				FN_attribute	attr(FN_identifier(FN_ID_STRING,
						    strlen(at), at), ascii);

				// %%% handle non-ASCII attribute values

				if (values = ldap_get_values_len(ld, entry,
				    at)) {

					for (i = 0; values[i] != NULL; i++) {

						attr.add(FN_attrvalue(
						    values[i]->bv_val,
					(unsigned int)(values[i]->bv_len)));
					}
					ldap_value_free_len(values);
				}
				attr_set->add(attr);
			}
			x500_trace("LDAPData::ldap_entries_to_set: %d %s\n", a,
			(a == 1) ? "attribute" : "attributes");
		}

		// append to set

		switch (set_type) {

		case XFN_NAME_SET:
			name_set->add(FN_string(xrdn));
			break;

		case XFN_BINDING_SET:
			binding_set->add(FN_string(xrdn), *ref);
			delete ref;
			ref = 0;
			break;

		case XFN_ATTR_SET:
			continue;	// finished

		case XFN_SEARCH_SET:
			search_set->add(FN_string(xrdn), ref ? ref : 0,
			    attr_set ? attr_set : 0);
			delete attr_set;
			delete ref;
			attr_set = 0;
			ref = 0;
			break;

		case XFN_EXT_SEARCH_SET:
			ext_search_set->add(*dn_string, ref ? ref : 0,
			    attr_set ? attr_set : 0, xrdn ? 1 : 0);
			delete dn_string;
			delete attr_set;
			delete ref;
			dn_string = 0;
			attr_set = 0;
			ref = 0;
			break;

		default:
			return (FN_E_UNSPECIFIED_ERROR);
		}

		delete [] xdn;
		delete [] xrdn;
		xdn = 0;
		xrdn = 0;
		if (rdns)
			ldap_value_free(rdns);
		free(dn);
	}
	return (FN_SUCCESS);


cleanup:
	switch (set_type) {
	case XFN_NAME_SET:
		delete name_set;
		break;

	case XFN_BINDING_SET:
		delete [] xdn;
		delete [] xrdn;
		delete ref;
		if (rdns)
			ldap_value_free(rdns);
		free(dn);
		delete binding_set;
		break;

	case XFN_ATTR_SET:
		delete attr_set;
		break;

	case XFN_SEARCH_SET:
		delete [] xdn;
		delete [] xrdn;
		delete attr_set;
		if (rdns)
			ldap_value_free(rdns);
		free(dn);
		delete search_set;
		break;

	case XFN_EXT_SEARCH_SET:
		delete dn_string;
		delete dots;
		delete [] xdn;
		delete [] xrdn;
		delete attr_set;
		if (rdns)
			ldap_value_free(rdns);
		free(dn);
		delete ext_search_set;
	}
	return (err);
}
