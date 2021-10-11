/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fncreate_attr_utils.cc	1.1	96/03/31 SMI"

#include <xfn/xfn.hh>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#include "fncreate_attr_utils.hh"

static
FN_attribute *
make_ctxtype_attr(unsigned int context_type)
{
	static FN_identifier
		UINT_SYNTAX((const unsigned char *)"fn_attr_syntax_uint");
	static FN_identifier
		CTXTYPE_ATTR_ID((const unsigned char *)"fn_context_type");

	FN_attrvalue cval((const void *)&(context_type),
			    sizeof (unsigned int));
	FN_attribute *attr = new FN_attribute(CTXTYPE_ATTR_ID, UINT_SYNTAX);

	if (attr) {
		attr->add(cval);
	}
	return (attr);
}

static
FN_attribute *
make_reftype_attr(const FN_identifier *ref_type)
{
	static FN_identifier
		FNID_SYNTAX((const unsigned char *)"fn_attr_fn_identifier");
	static FN_identifier
		REFTYPE_ATTR_ID((const unsigned char *)"fn_reference_type");

	size_t len = sizeof (FN_identifier_t) + ref_type->length();
	char *buf = (char *)malloc(len);

	memcpy(buf, (const void *)(&(ref_type->info)),
	    sizeof (FN_identifier_t));
	memcpy(buf + sizeof (FN_identifier_t), ref_type->contents(),
	    ref_type->length());

	FN_attrvalue rval((const void *)buf, len);
	FN_attribute *attr = new FN_attribute(REFTYPE_ATTR_ID, FNID_SYNTAX);

	free(buf);

	if (attr) {
		attr->add(rval);
	}
	return (attr);
}


FN_attrset *
generate_creation_attrs(unsigned int context_type,
    const FN_identifier *ref_type)
{
	FN_attrset *attrs = new FN_attrset();
	FN_attribute* ctxtype_attr;
	FN_attribute* reftype_attr;

	if (attrs == NULL)
		return (NULL);

	if ((ctxtype_attr = make_ctxtype_attr(context_type)) != NULL) {
		attrs->add(*ctxtype_attr);
		delete ctxtype_attr;
	} else {
		delete attrs;
		return (NULL);
	}

	if (ref_type &&
	    ((reftype_attr = make_reftype_attr(ref_type)) != NULL)) {
		attrs->add(*reftype_attr);
		delete reftype_attr;
	}

	return (attrs);
}
