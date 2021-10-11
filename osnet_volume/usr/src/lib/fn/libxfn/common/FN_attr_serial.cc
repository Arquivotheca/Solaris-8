/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_attr_serial.cc	1.7	97/04/29 SMI"

#include <stdlib.h>	/* malloc */
#include <string.h>	/* memset */
#include <rpc/rpc.h>	/* XDR routines */

#include <xfn/xfn.hh>
#include <xfn/fn_xdr.hh>
#include "FN_attr_serial.h"

// xdr_FN_attr
// routine to xdr a FN_attrset class into an XDR stream

bool_t
xdr_FN_attr(XDR *xdrs, FN_attrset **attrset)
{
	xFN_attrset	x_as;
	xFN_attribute	*x_attr;
	xFN_attrvalue   *x_attrvalue;
	bool_t 		status = FALSE;

	void		*iterpos, *pos;
	int		howmany, num, i, j;

	const FN_attribute 	*attr;
	const FN_attrvalue	*attrvalue;

	// encoding
	memset(&x_as, 0, sizeof (xFN_attrset));

	switch (xdrs->x_op) {
	case XDR_ENCODE:
	{
		// check for attribute set
		if (*attrset == NULL)
			return (FALSE);

		howmany = (int) (*attrset)->count();
		x_as.attributes.attributes_len = howmany;
		x_attr = x_as.attributes.attributes_val =
		    (xFN_attribute *)malloc(sizeof (xFN_attribute) * howmany);

		/* Copy pointers from attributes to temporary structure */
		attr = (*attrset)->first(iterpos);
		if (attr == NULL)
			goto cleanup_encode;
		for (i = 0; attr && i < howmany; i++) {
			const FN_identifier	*atype;

			atype = attr->identifier();
			x_attr[i].type.format = atype->format();
			x_attr[i].type.contents.contents_val =
				(char *) atype->contents();
			x_attr[i].type.contents.contents_len =
				atype->length();

			atype = attr->syntax();
			x_attr[i].syntax.format = atype->format();
			x_attr[i].syntax.contents.contents_val =
				(char *) atype->contents();
			x_attr[i].syntax.contents.contents_len =
				atype->length();

			num = attr->valuecount();
			x_attr[i].values.values_len = num;
			x_attrvalue = x_attr[i].values.values_val =
			    (xFN_attrvalue *)malloc(
			    (sizeof (xFN_attrvalue)) * num);

			attrvalue = attr->first(pos);
			for (j = 0; attrvalue && j < num; j++) {
				x_attrvalue[j].value.value_len =
				    attrvalue->length();
				x_attrvalue[j].value.value_val =
				    (char *)attrvalue->contents();
				attrvalue = attr->next(pos);
			}
			attr = (*attrset)->next(iterpos);
		}

		// actually xdr the FN_ref structure
		status = xdr_xFN_attrset(xdrs, &x_as);

	cleanup_encode:
		if (x_attr) {
			for (i = 0; i < howmany; i++) {
				x_attrvalue = x_attr[i].values.values_val;
				if (x_attrvalue)
					free(x_attrvalue);
			}
			free(x_attr);
		}
		return (status);
	}
	case XDR_DECODE:
		FN_attribute *attribute;
		FN_attrvalue *atvalue;

		*attrset = 0;
		status = xdr_xFN_attrset(xdrs, &x_as);
		if (status == FALSE)
			goto cleanup_decode;

		if (!(*attrset = new FN_attrset()))
			goto cleanup_decode;

		howmany = x_as.attributes.attributes_len;
		x_attr = x_as.attributes.attributes_val;
		for (i = 0; i < howmany; i++) {
			FN_identifier	*atype, *stype;

			atype = new FN_identifier(x_attr[i].type.format,
			    x_attr[i].type.contents.contents_len,
			    x_attr[i].type.contents.contents_val);
			if (!atype) goto cleanup_decode;
			stype = new FN_identifier(x_attr[i].syntax.format,
			    x_attr[i].syntax.contents.contents_len,
			    x_attr[i].syntax.contents.contents_val);
			if (!stype) goto cleanup_decode;
			attribute = new FN_attribute((*atype), (*stype));
			if (!attribute) goto cleanup_decode;
			delete atype;
			delete stype;

			num = x_attr[i].values.values_len;
			x_attrvalue = x_attr[i].values.values_val;
			for (j = 0; j < num; j++) {
				atvalue = new FN_attrvalue((const void *)
				    x_attrvalue[j].value.value_val,
				    (size_t) x_attrvalue[j].value.value_len);
				attribute->add((const FN_attrvalue) (*atvalue));
				delete atvalue;
			}

			(*attrset)->add((const FN_attribute) (*attribute));
			delete attribute;
		}

	cleanup_decode:
		// free any memory allocated
		xdr_free((xdrproc_t)xdr_xFN_attrset, (char *)&x_as);
		if (status == FALSE && *attrset)
			delete(*attrset);
		return (status);


	case XDR_FREE:
		// can't delete the FN_ref class using xdr_free
		// should use delete of the FN_ref pointer
		return (FALSE);

	default:
		return (FALSE);
	}
}


char *
FN_attr_xdr_serialize(const FN_attrset &attrset, int &bufsize)
{
	XDR	xdrs;
	char	*buf;
	bool_t 	status;

	FN_attrset	*a = (FN_attrset *) &attrset;

	// Calculate size and allocate space for result
	int size = (int) xdr_sizeof((xdrproc_t)xdr_FN_attr, &a);

	buf = (char *)malloc(size);
	if (buf == NULL)
		return (0);

	// XDR structure into buffer
	xdrmem_create(&xdrs, buf, size, XDR_ENCODE);
	status = xdr_FN_attr(&xdrs, &a);

	if (status == FALSE) {
		free(buf);
		return (0);
	}
	bufsize = size;
	return (buf);
}



FN_attrset *
FN_attr_xdr_deserialize(const char *buf, const int bufsize, unsigned &status)
{
	FN_attrset *attrset = 0;
	XDR 	xdrs;
	bool_t	res;

	if ((buf == NULL) || (bufsize == 0)) {
		status = FN_SUCCESS;
		return (0);
	}
	xdrmem_create(&xdrs, (caddr_t)buf, bufsize, XDR_DECODE);
	res = xdr_FN_attr(&xdrs, &attrset);
	if (res == FALSE)
		status = FN_E_MALFORMED_REFERENCE;
	else
		status = FN_SUCCESS;
	return (attrset);
}
