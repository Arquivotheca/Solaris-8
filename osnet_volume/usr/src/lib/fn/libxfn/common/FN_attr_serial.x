/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

/*
 * ident "@(#)FN_attr_serial.x	1.1 94/07/25 SMI"
 */

%#include "FN_ref_serial.h"

struct xFN_attrvalue
{
	opaque value<>;
};

struct xFN_attribute
{
	xFN_identifier 	type;
	xFN_identifier 	syntax;
	xFN_attrvalue	values<>;
};

struct xFN_attrset
{
	xFN_attribute attributes<>;
};

