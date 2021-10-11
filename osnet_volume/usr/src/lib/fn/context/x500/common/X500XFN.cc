/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)X500XFN.cc	1.4	96/04/22 SMI"


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>	// isspace()
#include "X500XFN.hh"


/*
 * XFN data structure manipulation
 */


// useful XFN identifiers
const FN_identifier	X500XFN::x500(
			    (const unsigned char *)"x500");
const FN_identifier	X500XFN::paddr(
			    (const unsigned char *)"presentationAddress");
const FN_identifier	X500XFN::ascii(
			    (const unsigned char *)"fn_attr_syntax_ascii");
const FN_identifier	X500XFN::octet(
			    (const unsigned char *)"fn_attr_syntax_octet");

// useful XFN constants
const int		X500XFN::max_ref_length = 1024; // reference string
const int		X500XFN::max_filter_length = 2048; // filter string
const int		X500XFN::max_stack_length = 128; // stack of boolean ops
const int		X500XFN::max_dn_length = 1024; // X/Open DCE Dir. form


/*
 * Convert a string to the format specifier in an FN_identifier.
 */
unsigned char *
X500XFN::string_to_id_format(
	unsigned char *cp,
	unsigned int &format
) const
{
	if (strncmp((char *)cp, "id$", 3) == 0) {
		format = FN_ID_STRING;
		return (cp += 3);
	} else if (strncmp((char *)cp, "oid$", 4) == 0) {
		format = FN_ID_ISO_OID_STRING;
		return (cp += 4);
	} else if (strncmp((char *)cp, "uuid$", 5) == 0) {
		format = FN_ID_DCE_UUID;
		return (cp += 5);
	} else
		return (0);
}


/*
 * Convert the format specifier in an FN_identifier to a string.
 */
unsigned char *
X500XFN::id_format_to_string(
	unsigned int	format,
	unsigned char	*cp
) const
{
	switch (format) {
	case FN_ID_STRING:
		strcpy((char *)cp, "id$");
		return (cp += 3);

	case FN_ID_ISO_OID_STRING:
		strcpy((char *)cp, "oid$");
		return (cp += 4);

	case FN_ID_DCE_UUID:
		strcpy((char *)cp, "uuid$");
		return (cp += 5);

	default:
		return (0);
	}
}


/*
 * Convert a string reference (nNSReferenceString or objectReferenceString)
 * into an XFN reference.
 *
 * The string reference has the following format:
 *
 * <id-tag> '$' <ref-id> { '$' <id-tag> '$' <addr-id> '$' <addr> }+
 *
 * where: { x }+ denotes one or more occurrences of x
 *        <id-tag> is one of 'id', 'oid', 'uuid' or 'ber'
 *        <ref-id> is a reference type
 *        <addr-id> is a reference address type
 *        <addr> is an address (hexadecimal string encoding)
 *
 * e.g.   "id$xxxx$id$yyyy$7a7a7a7a"
 *
 * NOTE: if a reference has an OID format address type and has the
 *       address value '\x00' then that address type is added to a list
 *       and the actual address value is fetched later and then appended
 *       to the XFN reference.
 *
 *       e.g.   "id$xxxx$oid$2.5.4.29$00"
 */
FN_ref *
X500XFN::string_ref_to_ref(
	unsigned char	*sref,
	unsigned int	len,
	FN_attrset	**attrs,
	int		&err
) const
{
	FN_ref		*ref;
	unsigned char   *ref_string;
	unsigned char   *cp;
	unsigned char   *cp2;
	unsigned int    format;

	if (! (cp = ref_string = new unsigned char [len + 1])) {
		err = FN_E_MALFORMED_REFERENCE;
		return (0);
	}
	memcpy(cp, sref, (size_t)len);
	ref_string[len] = '\0'; // add terminator

	// build ref-type

	if (! (cp = string_to_id_format(cp, format))) {
		delete [] ref_string;
		err = FN_E_MALFORMED_REFERENCE;
		return (0);
	}
	cp2 = cp;
	while (*cp && (*cp != '$')) {
		cp++;
	}
	FN_identifier   ref_id(format, cp - cp2, cp2);

	if ((ref = new FN_ref(ref_id)) == 0) {
		delete [] ref_string;
		err = FN_E_MALFORMED_REFERENCE;
		return (0);
	}

	// build ref-addr-type

	while (*cp) {
		if (*cp == '$')
			cp++;
		if (! (cp = string_to_id_format(cp, format))) {
			delete [] ref_string;
			delete ref;
			if (attrs)
				delete *attrs;
			err = FN_E_MALFORMED_REFERENCE;
			return (0);
		}
		cp2 = cp;
		while (*cp && (*cp != '$')) {
			cp++;
		}
		FN_identifier   addr_id(format, cp - cp2, cp2);
		if (*cp == '$')
			cp++;
		cp2 = cp;
		while (*cp && (*cp != '$')) {
			cp++;
		}

		int		buflen = (cp - cp2) / 2;
		unsigned char   *buf = new unsigned char [buflen];
		unsigned char   *bp = buf;

		// copy address data (decode hex string)

		unsigned char   hex_pair[3];

		hex_pair[2] = '\0';
		cp = cp2;
		while (*cp && (*cp != '$')) {

			hex_pair[0] = *cp++;
			if (*cp) {
				hex_pair[1] = *cp++;
			} else {
				delete [] buf;
				delete [] ref_string;
				if (attrs)
					delete *attrs;
				delete ref;
				err = FN_E_MALFORMED_REFERENCE;
				return (0);
			}
			*bp++ = (unsigned char)strtol((char *)hex_pair,
			    (char **)NULL, 16);
		}

		FN_ref_addr	ref_addr(addr_id, buflen, buf);

		// build a list of attributes to be fetched later
		if ((buflen == 1) && (buf[0] == '\0') &&
		    (format == FN_ID_ISO_OID_STRING)) {
			if (! *attrs)
				*attrs = new FN_attrset;

			// syntax is ignored, set it to ASCII for now
			(*attrs)->add(FN_attribute(addr_id, ascii));
		} else {
			ref->append_addr(ref_addr);
		}
		delete [] buf;
	}
	delete [] ref_string;

	return (ref);
}


/*
 * Convert an XFN reference into a string reference (nNSReferenceString or
 * objectReferenceString).
 *
 * The string reference has the following format:
 *
 * <id-tag> '$' <ref-id> { '$' <id-tag> '$' <addr-id> '$' <addr> }+
 *
 * where: { x }+ denotes one or more occurrences of x
 *        <id-tag> is one of 'id', 'oid', 'uuid' or 'ber'
 *        <ref-id> is a reference type
 *        <addr-id> is a reference address type
 *        <addr> is an address (hexadecimal string encoding)
 *
 * e.g.   "id$xxxx$id$yyyy$7a7a7a7a"
 */
unsigned char *
X500XFN::ref_to_string_ref(
	const FN_ref	*ref,
	int		*length
) const
{
	const FN_identifier	*id = 0;
	unsigned char		*ref_string =
				    new unsigned char [max_ref_length];
	unsigned char		*cp = ref_string;
	const unsigned char	*cp2;
	void			*iter_pos;

	// copy reference type
	if (! (id = ref->type())) {
		delete [] ref_string;
		return (0);
	}
	if (! (cp = id_format_to_string(id->format(), cp))) {
		delete [] ref_string;
		return (0);
	}
	cp2 = id->str();
	strcpy((char *)cp, (char *)ref->type()->str());
	cp += strlen((char *)cp2);

	int			addr_num = ref->addrcount();
	const FN_ref_addr	*ref_addr = ref->first(iter_pos);

	x500_trace("X500XFN::ref_to_string_ref: ref-type: %s %s %d\n",
	    ref->type()->str(), "ref-addresses:", addr_num);

	while (addr_num--) {

		x500_trace("X500XFN::ref_to_string_ref: ref-addr-type: %s\n",
		    ref_addr->type()->str());

		*cp++ = '$';
		id = 0;

		// copy address type
		if (! (id = ref_addr->type())) {
			delete [] ref_string;
			return (0);
		}
		if (! (cp = id_format_to_string(id->format(), cp))) {
			delete [] ref_string;
			return (0);
		}
		cp2 = id->str();
		strcpy((char *)cp, (char *)cp2);
		cp += strlen((char *)cp2);

		*cp++ = '$';

		// copy address data (encode hex string)

		int 		i;
		int		len;
		unsigned char	*addr = (unsigned char *)ref_addr->data();

		len = ref_addr->length();
		for (i = 0; i < len; i++) {
			cp += sprintf((char *)cp, "%.2x", *addr++);
		}
		ref_addr = ref->next(iter_pos);
	}
	*length = cp - ref_string;

	x500_trace("X500XFN::ref_to_string_ref (%d): %s\n", cp - ref_string,
	    ref_string);

	return (ref_string);
}


/*
 * Convert an XFN attribute set and/or reference to an XFN modification list
 */
FN_attrmodlist *
X500XFN::attrs_and_ref_to_mods(
	const FN_attrset	*attrs,
	const FN_ref		*ref,
	int			&err
)
{
	FN_attrmodlist		*mods;
	const FN_attribute	*attr;
	void			*iter;
	unsigned int		mod_num;

	if (! (mods = new FN_attrmodlist)) {
		err = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}

	// handle attribute set
	for (attr = attrs->first(iter); attr != 0; attr = attrs->next(iter)) {
		mods->add(FN_ATTR_OP_ADD_EXCLUSIVE, *attr);
	}

	// handle reference
	if (ref) {
		FN_identifier	id((unsigned char *)"objectReferenceString");
		FN_attribute	ref_attr(id, ascii);
		unsigned char	*ref_str;
		int		len;

		if (! (ref_str = ref_to_string_ref(ref, &len))) {
			err = FN_E_MALFORMED_REFERENCE;
			delete mods;
			return (0);
		}

		ref_attr.add(FN_attrvalue(ref_str));
		mods->add(FN_ATTR_OP_ADD_EXCLUSIVE, ref_attr);

		delete [] ref_str;
	}
	mod_num = mods->count();

	x500_trace("X500XFN::attrs_to_ref_mods: %d modification%s\n", mod_num,
	    (mod_num == 1) ? "" : "s");

	return (mods);
}


/*
 * Test for Or boolean operator (scan right-to-left)
 */
int
X500XFN::is_or(
	const unsigned char	*fx
)
{
	int	i = 0;
	int	j = 0;

	while (isspace(*fx)) {
		i++;
		fx--;
	}
	if ((*fx == 'r') && (*(fx - 1) == 'o')) {
		j = 2;
		fx -= 2;
	}
	while (isspace(*fx)) {
		i++;
		fx--;
	}

	return (j ? i + j : 0);
}


/*
 * Test for And boolean operator (scan right-to-left)
 */
int
X500XFN::is_and(
	const unsigned char	*fx
)
{
	int	i = 0;
	int	j = 0;

	while (isspace(*fx)) {
		i++;
		fx--;
	}
	if ((*fx == 'd') && (*(fx - 1) == 'n') && (*(fx - 2) == 'a')) {
		j = 3;
		fx -= 3;
	}
	while (isspace(*fx)) {
		i++;
		fx--;
	}

	return (j ? i + j : 0);
}


/*
 * Test for Not boolean operator (scan right-to-left)
 */
int
X500XFN::is_not(
	const unsigned char	*fx
)
{
	int	i = 0;
	int	j = 0;

	while (isspace(*fx)) {
		i++;
		fx--;
	}
	if ((*fx == 't') && (*(fx - 1) == 'o') && (*(fx - 2) == 'n')) {
		j = 3;
		fx -= 3;
	}
	while (isspace(*fx)) {
		i++;
		fx--;
	}

	return (j ? i + j : 0);
}


/*
 * Test for Equal relational operator
 */
int
X500XFN::is_equal(
	const unsigned char	*fx
)
{
	int	i = 0;
	int	j = 0;

	while (isspace(*fx)) {
		i++;
		fx--;
	}
	if ((*fx == '=') && (*(fx - 1) == '=')) {
		j = 2;
		fx -= 2;
	}
	while (isspace(*fx)) {
		i++;
		fx--;
	}

	return (j ? i + j : 0);
}


/*
 * Test for Not-equal relational operator
 */
inline int
X500XFN::is_not_equal(
	const unsigned char	*fx
)
{
	int	i = 0;
	int	j = 0;

	while (isspace(*fx)) {
		i++;
		fx--;
	}
	if ((*fx == '=') && (*(fx - 1) == '!')) {
		j = 2;
		fx -= 2;
	}
	while (isspace(*fx)) {
		i++;
		fx--;
	}

	return (j ? i + j : 0);
}


/*
 * Test for Approx-equal relational operator
 */
int
X500XFN::is_approx_equal(
	const unsigned char	*fx
)
{
	int	i = 0;
	int	j = 0;

	while (isspace(*fx)) {
		i++;
		fx--;
	}
	if ((*fx == '=') && (*(fx - 1) == '~')) {
		j = 2;
		fx -= 2;
	}
	while (isspace(*fx)) {
		i++;
		fx--;
	}

	return (j ? i + j : 0);
}


/*
 * Test for Greater-or-equal relational operator
 */
int
X500XFN::is_greater_or_equal(
	const unsigned char	*fx
)
{
	int	i = 0;
	int	j = 0;

	while (isspace(*fx)) {
		i++;
		fx--;
	}
	if ((*fx == '=') && (*(fx - 1) == '>')) {
		j = 2;
		fx -= 2;
	}
	while (isspace(*fx)) {
		i++;
		fx--;
	}

	return (j ? i + j : 0);
}


/*
 * Test for Greater-than relational operator
 */
int
X500XFN::is_greater_than(
	const unsigned char	*fx
)
{
	int	i = 0;
	int	j = 0;

	while (isspace(*fx)) {
		i++;
		fx--;
	}
	if (*fx == '>') {
		j = 1;
		fx--;
	}
	while (isspace(*fx)) {
		i++;
		fx--;
	}

	return (j ? i + j : 0);
}


/*
 * Test for Less-or-equal relational operator
 */
int
X500XFN::is_less_or_equal(
	const unsigned char	*fx
)
{
	int	i = 0;
	int	j = 0;

	while (isspace(*fx)) {
		i++;
		fx--;
	}
	if ((*fx == '=') && (*(fx - 1) == '<')) {
		j = 2;
		fx -= 2;
	}
	while (isspace(*fx)) {
		i++;
		fx--;
	}

	return (j ? i + j : 0);
}


/*
 * Test for Less-than relational operator
 */
int
X500XFN::is_less_than(
	const unsigned char	*fx
)
{
	int	i = 0;
	int	j = 0;

	while (isspace(*fx)) {
		i++;
		fx--;
	}
	if (*fx == '<') {
		j = 1;
		fx--;
	}
	while (isspace(*fx)) {
		i++;
		fx--;
	}

	return (j ? i + j : 0);
}


/*
 * Add parentheses to an XFN filter expression (in prefix)
 *
 *     e.g. "|(%a=*z)&!(%a<=%s)(%a=*)"  ->  "(|(%a=*z)(&(!(%a<=%s))(%a=*)))"
 */
unsigned char *
X500XFN::parenthesize_filter_expression(
	unsigned char	*px,	// prefix expression
	unsigned char	*&ppx,	// parenthesized prefix expression
	int		operands
)
{
	while (*px) {

		if ((*px == '&') || (*px == '|') || (*px == '!')) {
			/* operator */

			*ppx++ = '(';
			*ppx++ = *px;
			if (*px != '!') {
				// AND, OR: require 2 operands
				px = parenthesize_filter_expression(++px, ppx,
				    2);
			} else {
				// NOT: requires 1 operand
				px = parenthesize_filter_expression(++px, ppx,
				    1);
			}
			operands--;
		} else {
			/* operand */

			while (*px && (*px != ')'))
				*ppx++ = *px++;

			if (*px == ')')
				*ppx++ = *px++;

			operands--;
		}

		if (operands == 0) {
			*ppx++ = ')';
			break;
		}
	}
	if (! *px)
		*ppx = '\0';

	return (px);
}


/*
 * Substitute any filter arguments into the body of the filter expression
 *
 *     e.g. "(&(%a=%s)(%i=*))"  ->  "(&(surname=*z)(telephoneNumber=*))"
 *
 *     where:  "%a" represents the surname attribute identifier
 *             "%s" represents the string "*'z'"
 *             "%i" represents the telephoneNumber attribute identifier
 */
unsigned char *
X500XFN::substitute_filter_arguments(
	unsigned char		*fx,	// filter expression
	const FN_search_filter	*filter,
	int			&err
)
{
	unsigned char			*fxa = 0;	// filter expression
							//  including arguments
	unsigned char			*overflow;
	const void			**fargs;	// filter arguments
	const FN_search_filter_type	*farg_t;	// filter argument types
	size_t				farg_num;	// number of arguments
	const FN_attribute_t		*attr_t;
	const FN_identifier_t		*id_t;
	const FN_identifier_t		*syntax_t;
	const FN_attrvalue_t		*av_t;
	const FN_string_t		*str_t;
	size_t				len;
	int				i = 0;
	const unsigned char		*cp;
	int				j;

	if ((! (fargs = filter->filter_arguments(&farg_num))) ||
	    (! (farg_t = filter->filter_argument_types(&farg_num)))) {
		delete [] fxa;
		err = FN_SUCCESS;	// no arguments to substitute
		return (0);
	}

	if (! (fxa = new unsigned char [max_filter_length])) {
		err = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}
	overflow = &fxa[max_filter_length - 1];


	x500_trace("X500XFN::substitute_filter_arguments: %d filter %s\n",
	    farg_num, (farg_num == 1) ? "argument" : "arguments");

	while (*fx) {

		// replace substitution tokens
		if (*fx == '%') {
			len = 0;
			fx++;
			switch (*fx++) {

			case 'a':
				if ((farg_t[i] == FN_SEARCH_FILTER_ATTR) &&
				    (attr_t = (FN_attribute_t *)fargs[i]) &&
				    (id_t = fn_attribute_identifier(attr_t)) &&
				    (syntax_t = fn_attribute_syntax(attr_t)) &&
				    (ascii == FN_identifier(*syntax_t)) &&
				    (((len = id_t->length) + fxa) < overflow)) {

					memcpy(fxa, id_t->contents, len);
					fxa += len;
				} else {
					if (syntax_t &&
					    (ascii != FN_identifier(*syntax_t)))
					{
						x500_trace("X500XFN::%s%s %s\n",
						    "substitute_filter_",
						    "arguments:",
						    "non-ASCII syntax");
					}
					err = FN_E_SEARCH_INVALID_FILTER;
				}
				break;

			case 'v':
				if ((farg_t[i] == FN_SEARCH_FILTER_ATTRVALUE) &&
				    (av_t = (FN_attrvalue_t *)fargs[i]) &&
				    (syntax_t) &&
				    (ascii == FN_identifier(*syntax_t)) &&
				    (((len = av_t->length) + fxa) < overflow)) {

					memcpy(fxa, av_t->contents, len);
					fxa += len;
				} else {
					if (syntax_t &&
					    (ascii != FN_identifier(*syntax_t)))
					{
						x500_trace("X500XFN::%s%s %s\n",
						    "substitute_filter_",
						    "arguments:",
						    "non-ASCII syntax");
					}
					err = FN_E_SEARCH_INVALID_FILTER;
				}
				break;

			case 's':
				if ((farg_t[i] == FN_SEARCH_FILTER_STRING) &&
				    (str_t = (FN_string_t *)fargs[i]) &&
				    (((len = fn_string_bytecount(str_t)) + fxa)
				    < overflow) &&
				    (cp = fn_string_str(str_t, 0))) {

					for (j = 0; j < len; j++) {
						if (*cp != '\'')
							*fxa++ = *cp++;
						else
							cp++;	// skip quotes
					}
				} else {
					err = FN_E_SEARCH_INVALID_FILTER;
				}
				break;

			case 'i':
				if ((farg_t[i] ==
				    FN_SEARCH_FILTER_IDENTIFIER) &&
				    (id_t = (FN_identifier_t *)fargs[i]) &&
				    (((len = id_t->length) + fxa) < overflow)) {

					memcpy(fxa, id_t->contents, len);
					fxa += len;
				} else {
					err = FN_E_SEARCH_INVALID_FILTER;
				}
				break;

			default:
				x500_trace("X500XFN:%s: invalid token\n",
				    "substitute_filter_arguments");
				err = FN_E_SEARCH_INVALID_FILTER;
			}
			i++;	// increment argument index

		} else {
			// overflow ?
			if (fxa < overflow) {
				*fxa++ = *fx++;
			} else {
				len = 1;
				err = FN_E_SEARCH_INVALID_FILTER;
			}
		}

		// error encountered
		if (err == FN_E_SEARCH_INVALID_FILTER) {
			if ((fxa + len) >= overflow) {
				x500_trace("X500XFN::%s: filter overflow\n",
				    "substitute_filter_arguments");
			}
			delete [] fxa;
			return (0);
		}
	}
	*fxa = '\0';
	fxa = overflow - (max_filter_length - 1);	// reset to beginning
	err = FN_SUCCESS;

	return (fxa);
}


/*
 * Convert an XFN filter expression (from infix) to prefix.
 *
 * Boolean operators are converted to prefix format and are mapped as follows:
 *     'or'   ->  '|'
 *     'and'  ->  '&'
 *     'not'  ->  '!'
 *
 * Relational operators remain in infix format and are mapped as follows:
 *     x '==' y  ->  x '=' y
 *     x '!=' y  ->  !(x '=' y)
 *     x '>' y   ->  !(x '<=' y)
 *     x '<' y   ->  !(x '>=' y)
 *
 * NOTE: extended operations are not supported.
 *
 * The order of operands is preserved. Single quotes are stripped from values.
 * The value '=*' is used to denote a test for the presence of an attribute.
 *
 *     e.g. "(%a == *'z') or (%a > %s) and %a"  ->  "|(%a=*z)&!(%a<=%s)(%a=*)"
 */
unsigned char *
X500XFN::filter_expression_to_prefix(
	const unsigned char	*ix,	// infix expression
	int			ix_len,
	unsigned char		*px,	// prefix expression
	int			px_len
)
{
	const unsigned char	*ixp = &ix[ix_len - 1];
	unsigned char		*pxp = &px[px_len - 1];
	unsigned char		stack[max_stack_length];
	unsigned char		*sp = &stack[0];
	int			negate;
	int			op_present;
	unsigned char		*marker1;
	unsigned char		*marker2;
	int			i = 0;

	*pxp-- = '\0';
	*sp = '\0';

	// scan right-to-left to preserve order of operands

	while (ix <= ixp) {

		if (*ixp == '(') {

			// pop stack until matching parenthesis
			while (*sp && (*sp != ')')) {
				*pxp-- = *sp--;
			}
			if (*sp == ')') {
				sp--;
			} else {
				return (0);	// unbalanced parantheses
			}
			ixp--;

		} else if (i = is_or(ixp)) {

			// pop stack while less or equal precedence
			while (*sp && ((*sp == '!') || (*sp == '&') ||
			    (*sp == '|'))) {
				*pxp-- = *sp--;
			}
			*++sp = '|';
			ixp -= i;

		} else if (i = is_and(ixp)) {

			// pop stack while less or equal precedence
			while (*sp && ((*sp == '!') || (*sp == '&'))) {
				*pxp-- = *sp--;
			}
			*++sp = '&';
			ixp -= i;

		} else if (i = is_not(ixp)) {

			// pop stack while less or equal precedence
			while (*sp && (*sp == '!')) {
				*pxp-- = *sp--;
			}
			*++sp = '!';
			ixp -= i;

		} else if (*ixp == ')') {

			*++sp = ')';
			ixp--;

		} else if (isspace(*ixp)) {

			ixp--;	// ignore spaces

		} else {
			// operand

			op_present = 0;
			negate = 0;
			*pxp-- = ')';
			marker1 = pxp;	// mark current position

			while (ix <= ixp) {

				// %%% support extended operations in filter

				if (i = is_equal(ixp)) {

					op_present++;
					*pxp-- = '=';
					ixp -= i;

				} else if (i = is_not_equal(ixp)) {

					op_present++;
					negate++;
					*pxp-- = '=';
					ixp -= i;

				} else if (i = is_approx_equal(ixp)) {

					op_present++;
					*pxp-- = '=';
					*pxp-- = '~';
					ixp -= i;

				} else if (i = is_greater_or_equal(ixp)) {

					op_present++;
					*pxp-- = '=';
					*pxp-- = '>';
					ixp -= i;

				} else if (i = is_less_or_equal(ixp)) {

					op_present++;
					*pxp-- = '=';
					*pxp-- = '<';
					ixp -= i;

				} else if (i = is_greater_than(ixp)) {

					op_present++;
					negate++;
					*pxp-- = '=';
					*pxp-- = '<';
					ixp -= i;

				} else if (i = is_less_than(ixp)) {

					op_present++;
					negate++;
					*pxp-- = '=';
					*pxp-- = '>';
					ixp -= i;

				} else if (*ixp == '\'') {

					ixp--;	// strip quotes

				} else if ((*ixp == '(') || (is_and(ixp)) ||
				    (is_or(ixp)) || (is_not(ixp))) {

					break;	// end of operand

				} else {
					*pxp-- = *ixp--;
				}
			}
			// add '=*' if no value present
			if (! op_present) {
				marker2 = pxp;	// mark current position

				// make room for '=*'
				pxp++;
				while (pxp <= marker1) {
					*(pxp - 2) = *pxp;
					pxp++;
				}
				*--pxp = '*';
				*--pxp = '=';
				pxp = marker2 - 2;
			}
			*pxp-- = '(';
			if (negate)
				*pxp-- = '!';

		}
	}

	// pop remaining stack
	while (*sp) {
		*pxp-- = *sp--;
	}
	pxp++;

	return (pxp);
}


/*
 * Convert XFN filter into a string
 *
 * 1. convert XFN filter expression into LDAP prefix format.
 * 2. add parentheses to the filter expression.
 * 3. substitute any filter arguments into the body of the filter expression.
 *
 * NOTE: the string produced is in the LDAP filter format (RFC 1558).
 */
unsigned char *
X500XFN::filter_to_string(
	const FN_search_filter	*filter,
	int			&err
)
{
	const unsigned char	*fx;	// filter expression
	unsigned char		*pfx;	// prefix filter expression
	unsigned char		*pfx2;
	unsigned char		*ppfx;	// parenthesised prefix filter
					//  expression
	unsigned char		*ppfx2;
	unsigned char		*appfx;	// parenthesised prefix filter
					//  expression with arguments
	size_t			fx_len;

	if ((! filter) || ((fx = filter->filter_expression()) && (! *fx))) {
		x500_trace("X500XFN::filter_to_string: empty filter\n");
		return ((unsigned char *)strcpy(new char[14], "objectClass=*"));
	}

	if (! fx) {
		err = FN_E_SEARCH_INVALID_FILTER;
		return (0);
	}
	x500_trace("X500XFN::filter_to_string: %s\n", fx);

	// convert XFN filter expression to prefix
	pfx = ppfx = 0;
	fx_len = strlen((char *)fx);
	if ((! (pfx = new unsigned char [fx_len + 1])) ||
	    (! (ppfx = new unsigned char [fx_len * 2]))) {
		delete [] pfx;
		delete [] ppfx;
		err = FN_E_INSUFFICIENT_RESOURCES;
		return (0);
	}

	if (! (pfx2 = filter_expression_to_prefix(fx, fx_len, pfx,
	    fx_len + 1))) {
		delete [] pfx;
		delete [] ppfx;
		err = FN_E_SEARCH_INVALID_FILTER;
		return (0);
	}

	// add parentheses to prefix expression
	ppfx2 = ppfx;	// make a copy because passed by reference
	parenthesize_filter_expression(pfx2, ppfx2, 0);
	delete [] pfx;

	// substitute any filter arguments
	if (! (appfx = substitute_filter_arguments(ppfx, filter, err))) {
		if (err != FN_SUCCESS) {
			delete [] ppfx;
			return (0);
		}
		// no filter arguments to substitute (keep current string)

	} else {
		// filter arguments substituted (replace current string)
		delete [] ppfx;
		ppfx = appfx;
	}
	x500_trace("X500XFN::filter_to_string: %s\n", ppfx);

	return (ppfx);
}
