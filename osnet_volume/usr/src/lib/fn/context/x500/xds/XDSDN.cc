/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)XDSDN.cc	1.1	96/03/31 SMI"


#include <string.h>
#include <stdio.h>	// sprintf()
#include <stdlib.h>	// bsearch()
#include <ctype.h>	// isalnum()
#include "XDSExtern.hh"
#include "XDSDN.hh"


/*
 * XDS Distinguished Name
 */


// string distinguished name (X/Open DCE Directory form)
const int	max_dn_length = 1024;


/*
 * Build a distinguished name in XDS format (class DS_C_DS_DN)
 */
XDSDN::XDSDN(
	OM_descriptor	*name
)
{
	dn = name;
	delete_me = 0;
}


/*
 * Build a distinguished name in XDS format (class DS_C_DS_DN)
 */
XDSDN::XDSDN(
	const FN_string	&name
)
{
	unsigned char	*dn_string;
	unsigned char	*cp;
	int		rdn_num = 0;
	int		ava_num = 0;
	int		mul_ava_num = 0;
	int		idn;
	int		irdn;
	int		iava;
	int		multiple_ava;
	int		is_root = 0;


	// make a copy of the supplied name string
	int	len = name.bytecount();

	dn = 0;
	if (! (cp = new unsigned char [len + 1])) {
		return;
	}
	dn_string = (unsigned char *)memcpy(cp, name.str(), (size_t)len + 1);

	// count number of AVAs and number of multiple AVAs
	while (*cp) {
		if (*cp == '=')		// equal-sign separates RDNs
			ava_num++;
		if (*cp == ',')		// comma separates multiple AVAs
			mul_ava_num++;
		cp++;
	}

	if (ava_num < mul_ava_num) {

		x500_trace("[XDSDN] %d AVAs, (incl. %d multiple AVAs)\n",
		    ava_num, mul_ava_num);

		delete [] dn_string;
		return;
	}


	rdn_num = ava_num - mul_ava_num;
	idn = 0;					// start of DN object
	irdn = rdn_num + 2;				// start of RDN objects
	iava = rdn_num * 3 + mul_ava_num + irdn;	// start of AVA objects
	if ((dn = new OM_descriptor [ava_num * 4 + iava]) == 0) {
		delete [] dn_string;
		return;
	}
	delete_me = 1;

	int	total = ava_num *4 + iava;

	x500_trace("[XDSDN] %d RDNs, %d AVAs\n", rdn_num, ava_num);

	cp = dn_string;	// reset to start of name

	while (*cp == '.')	// skip over '...', if present
		cp++;
	if (*cp == '/') {	// skip over leading slash, if present
		cp++;
		if (! *cp)
			is_root = 1;	// ROOT is denoted by a single slash
	}

	while (*cp || is_root) {

		// build a DS_C_DS_DN object

		fill_om_desc(dn[idn++], OM_CLASS, DS_C_DS_DN);

		while (*cp) {

			fill_om_desc(dn[idn++], DS_RDNS, &dn[irdn]);

			// build a DS_C_DS_RDN object

			fill_om_desc(dn[irdn++], OM_CLASS, DS_C_DS_RDN);

			multiple_ava = 1;
			while (*cp && multiple_ava) {

				fill_om_desc(dn[irdn++], DS_AVAS,
				    &dn[iava]);

				// build a DS_C_AVA object
				int	quote = 0;

				// skip over any spaces
				while (isspace(*cp))
					cp++;

				if (*cp == '"') {
					quote = 1;
					cp++;
				}
				// skip over any spaces
				while (isspace(*cp))
					cp++;

				fill_om_desc(dn[iava++], OM_CLASS, DS_C_AVA);

				unsigned char	*cp2 = (unsigned char *)strchr
						    ((const char *)cp, '=');
				unsigned char	*cp3;

				if (! cp2) {
					delete [] dn_string;
					delete [] dn;
					dn = 0;
					return;
				}
				// backup over any spaces
				cp3 = cp2;
				while (isspace(*(cp3 - 1)))
					cp3--;

				*cp3 = '\0';

				OM_syntax		s;
				OM_object_identifier	*oid;

				if (! (oid = abbrev_to_om_oid((const char *)cp,
				    s))) {
					if ((! (oid = string_to_om_oid(cp))) ||
					    (! om_oid_to_syntax(oid, &s, 0))) {

						delete [] dn_string;
						delete [] dn;
						dn = 0;
						return;
					}
				}
				fill_om_desc(dn[iava++], DS_ATTRIBUTE_TYPE,
				    *oid);

				delete oid;	// delete contents later

				cp = ++cp2;
				while (*cp2) {
					if ((*cp2 != ',') && (*cp2 != '/')) {
						cp2++;
					} else {
						if ((quote == 1) &&
						    (*cp2 == '/')) {
							quote = 2;
							cp2++;
						} else {
							break;
						}
					}
				}

				if (*cp2 != ',')
					multiple_ava = 0;
				if (*cp2) {
					*cp2 = '\0';
					cp2--;
					if (quote && (*cp2 == '"'))
						*cp2 = '\0';
					cp2++;
				} else {
					cp2--;
					if (quote && (*cp2 == '"'))
						*cp2 = '\0';
				}

				fill_om_desc(dn[iava++], DS_ATTRIBUTE_VALUES, s,
				    (void *)cp, OM_LENGTH_UNSPECIFIED);

				cp = cp2 + 1;

				fill_om_desc(dn[iava++]);
			}
			fill_om_desc(dn[irdn++]);
		}
		fill_om_desc(dn[idn]);
		is_root = 0;
	}

	OM_private_object	pri_dn = 0;

	// convert to private object
	if (om_create(DS_C_DS_DN, OM_FALSE, workspace, &pri_dn) == OM_SUCCESS) {

		if (om_put(pri_dn, OM_REPLACE_ALL, dn, 0, 0, 0) != OM_SUCCESS) {
			om_delete(pri_dn);
		}
	}
	// cleanup attribute types
	int	i;
	for (i = 0; i < total; i++)
		if (dn[i].type == DS_ATTRIBUTE_TYPE)
			delete [] dn[i].value.string.elements;

	delete [] dn_string;
	delete [] dn;

	dn = pri_dn;
}


/*
 * Remove distinguished name in XDS format (class DS_C_DS_DN)
 */
XDSDN::~XDSDN(
)
{
	x500_trace("[~XDSDN]\n");

	if (dn && delete_me)
		om_delete(dn);
}


/*
 * Return a (relative) distinguished name in the format specified in
 * X/Open DCE Directory
 */
unsigned char *
XDSDN::str(
	int	is_dn	// DN or RDN
)
{
	unsigned char		*dn_string = new unsigned char [max_dn_length];
	unsigned char		*cp = dn_string;
	OM_public_object	pub_dn = 0;
	OM_value_position	total;
	OM_descriptor		*dnd;
	OM_descriptor		*rdnd;
	OM_descriptor		*avad;
	OM_descriptor		*avad2;
	int			multiple_ava;

	if (! dn_string)
		return (0);

	// convert to public
	if (dn->type == OM_PRIVATE_OBJECT) {
		if (om_get(dn, OM_NO_EXCLUSIONS, 0, OM_FALSE, 0, 0, &pub_dn,
		    &total) != OM_SUCCESS) {
			delete [] dn_string;
			return (0);
		}
		dn = pub_dn;
	}
	rdnd = dnd = dn;

	dnd++;	// skip OM_CLASS
	while (dnd->type != OM_NO_MORE_TYPES) {

		if (dnd->type == DS_RDNS)
			rdnd = dnd->value.object.object;

		rdnd++;	// skip OM_CLASS
		multiple_ava = 0;
		while (rdnd->type != OM_NO_MORE_TYPES) {

			if (multiple_ava)
				*cp++ = ',';	// comma separates multiple AVAs
			else
				if (is_dn)
					*cp++ = '/';	// slash separates RDNs

			if (rdnd->type == DS_AVAS)
				avad = rdnd->value.object.object;

			avad++;	// skip OM_CLASS
			if (avad->type != OM_NO_MORE_TYPES) {

				avad2 = avad;
				while ((avad->type != DS_ATTRIBUTE_TYPE) &&
				    (avad->type != OM_NO_MORE_TYPES))
					avad++;

				if (avad->type == DS_ATTRIBUTE_TYPE) {
					cp = om_oid_to_abbrev(
					    avad->value.string, cp);
				}

				avad = avad2;
				while ((avad->type != DS_ATTRIBUTE_VALUES) &&
					(avad->type != OM_NO_MORE_TYPES))
					avad++;

				if (avad->type == DS_ATTRIBUTE_VALUES) {

					switch (avad->syntax & OM_S_SYNTAX) {
					case OM_S_PRINTABLE_STRING:
					case OM_S_TELETEX_STRING:
					case OM_S_IA5_STRING:
					case OM_S_NUMERIC_STRING:
					case OM_S_VISIBLE_STRING:

						memcpy(cp,
						    avad->value.string.elements,
						    (size_t)
						    avad->value.string.length);
						cp = cp +
						avad->value.string.length;
						break;

					default:
						*cp++ = '?';
						break;
					}
				}
			}
			rdnd++;
			multiple_ava = 1;
		}
		dnd++;
	}
	*cp++ = '\0';

	if (pub_dn)
		om_delete(pub_dn);

	int	len = cp - dn_string;

	cp = dn_string;
	if (! (cp = new unsigned char [len])) {
		return (0);
	}
	memcpy(cp, dn_string, (size_t)len);

	x500_trace("XDSDN::str(): %s\n", cp);

	delete [] dn_string;
	return (cp);
}
