/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_ref_serial.cc	1.9	97/04/29 SMI"

#include <stdlib.h>	/* malloc */
#include <rpc/rpc.h>	/* XDR routines */

#include <xfn/xfn.hh>
#include <xfn/fn_xdr.hh>
#include "FN_ref_serial.h"


// xdr_FN_ref
// routine to xdr a FN_ref class into an XDR stream


bool_t
xdr_FN_ref(XDR *xdrs, FN_ref **ref)
{
	xFN_ref			rd;
	void			*iterpos;
	int			howmany, i;
	xFN_ref_addr		*addrs = 0;
	bool_t 			status = FALSE;

	// encoding
	memset(&rd, 0, sizeof (xFN_ref));

	switch (xdrs->x_op) {

	case XDR_ENCODE:
	{
		const FN_identifier	*rtype;
		const FN_ref_addr	*address;
		const FN_identifier	*atype_id;

		// get reference type string
		if (*ref == NULL)
			return (FALSE);
		rtype = (*ref)->type();
		rd.type.format = rtype->format();
		rd.type.contents.contents_val = (char *)rtype->contents();
		rd.type.contents.contents_len = rtype->length();

		howmany = rd.addrs.addrs_len = (*ref)->addrcount();
		addrs = rd.addrs.addrs_val =
		    (xFN_ref_addr *)malloc(sizeof (xFN_ref_addr) * howmany);
		if (addrs == NULL)
			goto cleanup_encode;

		/* Copy pointers from Address to temporary structure */
		address = (const FN_ref_addr *)(*ref)->first(iterpos);
		for (i = 0; address && i < howmany; i++) {
			addrs[i].addr.addr_len = address->length();
			addrs[i].addr.addr_val = (char *)address->data();

			// get address type data
			atype_id = address->type();
			if (atype_id) {
				addrs[i].type.format = atype_id->format();
				addrs[i].type.contents.contents_val =
					(char *)atype_id->contents();
				addrs[i].type.contents.contents_len =
					atype_id->length();
			} else {
				addrs[i].type.format = 0;
				addrs[i].type.contents.contents_val = 0;
				addrs[i].type.contents.contents_len = 0;
			}

			address = (const FN_ref_addr *)(*ref)->next(iterpos);
		}

		// actually xdr the FN_ref structure
		status = xdr_xFN_ref(xdrs, &rd);
	cleanup_encode:
		if (addrs)
			free(addrs);
		// no need to free data within addrs or rd  because they
		// point to const data
		return (status);
	}

	case XDR_DECODE:
	{
		xFN_ref_addr	*addr_struct;
		FN_identifier	*rtype;
		FN_identifier	*atype_id;
		FN_ref_addr	*address;

		*ref = 0;
		addrs = 0;
		status = xdr_xFN_ref(xdrs, &rd);
		if (status == FALSE)
			goto cleanup_decode;

		// temporary until new FN_string constructor becomes available
		rtype = new FN_identifier(rd.type.format,
		    rd.type.contents.contents_len,
		    rd.type.contents.contents_val);
		if (rtype) {
			*ref = new FN_ref(*rtype);
			delete rtype;
		} else {
			*ref = new FN_ref(FN_identifier());
		}
		if (*ref == NULL)
			goto cleanup_decode;

		for (i = 0; i < rd.addrs.addrs_len; i++) {
			addr_struct = &(rd.addrs.addrs_val[i]);
			atype_id = new FN_identifier(addr_struct->type.format,
			    addr_struct->type.contents.contents_len,
			    addr_struct->type.contents.contents_val);
			if (!atype_id) {
				status = FALSE;
				goto cleanup_decode;
			}
			address = new FN_ref_addr(*atype_id,
			    addr_struct->addr.addr_len,
			    addr_struct->addr.addr_val);

			delete atype_id;
			if (!address) {
				status = FALSE;
				goto cleanup_decode;
			}
			if ((*ref)->append_addr(*address) == 0) {
				delete address;
				status = FALSE;
				goto cleanup_decode;
			}
			delete address;
		}

	cleanup_decode:
		// free any memory allocated
		xdr_free((xdrproc_t)xdr_xFN_ref, (char *)&rd);
		if (status == FALSE && *ref)
			delete(*ref);
		return (status);
	}

	case XDR_FREE:
		// can't delete the FN_ref class using xdr_free
		// should use delete of the FN_ref pointer
		return (FALSE);

	default:
		return (FALSE);
	}
}





char *
FN_ref_xdr_serialize(const FN_ref &ref, int &bufsize)
{
	XDR	xdrs;
	char	*buf;
	bool_t 	status;

	FN_ref	*r = (FN_ref *)&ref;

	// Calculate size and allocate space for result
	int size = (int) xdr_sizeof((xdrproc_t)xdr_FN_ref, &r);
	buf = (char *)malloc(size);
	if (buf == NULL)
		return (0);

	// XDR structure into buffer
	xdrmem_create(&xdrs, buf, size, XDR_ENCODE);
	status = xdr_FN_ref(&xdrs, &r);
	if (status == FALSE) {
		free(buf);
		return (0);
	}
	bufsize = size;
	return (buf);
}



FN_ref *
FN_ref_xdr_deserialize(const char *buf, const int bufsize, unsigned &status)
{
	FN_ref *ref = 0;
	XDR 	xdrs;
	bool_t	res;

	xdrmem_create(&xdrs, (caddr_t)buf, bufsize, XDR_DECODE);
	res = xdr_FN_ref(&xdrs, &ref);
	if (res == FALSE)
		status = FN_E_MALFORMED_REFERENCE;
	else
		status = FN_SUCCESS;
	return (ref);
}
