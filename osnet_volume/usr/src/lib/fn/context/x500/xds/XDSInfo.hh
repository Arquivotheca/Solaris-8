/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_XDSINFO_HH
#define	_XDSINFO_HH

#pragma ident	"@(#)XDSInfo.hh	1.1	96/03/31 SMI"


#include "X500XFN.hh"

extern "C" {

#include "xom.h"

}


/*
 * XDS information tables and data structures
 */


struct oid_to_string_t {
	OM_object_identifier	*oid;
	int			index;	// into string_oid_table
	OM_syntax		syntax;
	OM_object_identifier	*class_oid;
};

struct string_to_oid_t {
	char			*string;
	char			*abbrev;
	OM_object_identifier	*oid;
};


class XDSInfo : public X500XFN
{
	static const struct oid_to_string_t	oid_to_string_table[];
	static const int			oid_to_string_table_size;
	static const struct string_to_oid_t	string_to_oid_table[];
	static const int			string_to_oid_table_size;
	static const char			*xds_problems[];

	static const int			max_oid_length;


public:

	void			fill_om_desc(OM_descriptor &desc) const;

	void			fill_om_desc(OM_descriptor &desc,
				    const OM_type type, OM_descriptor *object)
				    const;

	void			fill_om_desc(OM_descriptor &desc,
				    const OM_type type, const OM_string	oid)
				    const;

	void			fill_om_desc(OM_descriptor &desc,
				    const OM_type type, const OM_syntax	syntax,
				    void *string, const OM_string_length length
				    = OM_LENGTH_UNSPECIFIED) const;

	void			fill_om_desc(OM_descriptor &desc,
				    const OM_type type, const OM_sint32 number,
				    const OM_syntax syntax = OM_S_ENUMERATION)
				    const;

	void			fill_om_desc(OM_descriptor &desc,
				    const OM_type type,
				    const OM_boolean boolean) const;

	OM_return_code		deep_om_get(OM_type_list route,
				    const OM_private_object original,
				    const OM_exclusions exclusions,
				    const OM_type_list included_types,
				    const OM_boolean local_strings,
				    const OM_value_position initial_value,
				    const OM_value_position limiting_value,
				    OM_public_object *copy,
				    OM_value_position *total_number) const;

	int			om_oid_to_syntax(OM_object_identifier *oid,
				    OM_syntax *syntax,
				    OM_object_identifier **class_oid) const;

	unsigned char		*om_oid_to_string(OM_object_identifier *oid)
				    const;

	OM_object_identifier	*string_to_om_oid(unsigned char *oid) const;

	unsigned char		*xds_problem_to_string(int) const;

	int			compare_om_oids(OM_object_identifier &oid1,
				    OM_object_identifier &oid2) const;

	unsigned char		*om_oid_to_string_oid(OM_object_identifier *oid)
				    const;

	OM_string		*string_oid_to_om_oid(const char *oid) const;

	OM_object_identifier	*abbrev_to_om_oid(const char *oid,
				    OM_syntax &syntax) const;

	unsigned char		*om_oid_to_abbrev(OM_object_identifier &om_oid,
				    unsigned char *oid) const;


	XDSInfo() {};
	virtual ~XDSInfo() {};
};

#endif	/* _XDSINFO_HH */
