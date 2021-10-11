/*
 *	xdr_nullptr.c
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)xdr_nullptr.c	1.5	92/09/21 SMI"

/*
 * xdr_nullptr.c
 *
 * This function is used to control serializing and de-serializing of the
 * database objects. Basically there are some pointers in the structure
 * that we don't bother to serialize because they are always NULL when we
 * deserialize them. This function then simply returns on encode operations
 * and stuffs NULL into the pointer passed when decoding.
 */
#include <rpc/rpc.h>

bool_t
xdr_nullptr(xdrs, objp)
	XDR	*xdrs;
	void	**objp;
{
	if (xdrs->x_op == XDR_DECODE) {
		*objp = NULL;
	}
	return (TRUE);
}
