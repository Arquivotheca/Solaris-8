/*
 * Copyright (c) 1984, 1996-1999 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)xdr_array.c	1.9	99/02/23 SMI" /* from SunOS 4.1: 1.14 90/03/30 */

/*
 * xdr_array.c, Generic XDR routines impelmentation.
 *
 * Modified for use in the boot program.
 *
 * These are the "non-trivial" xdr primitives used to serialize and de-serialize
 * arrays.  See xdr.h for more info on the interface to xdr.
 */

#include <sys/types.h>
#include <sys/salib.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <sys/promif.h>
#include <sys/bootdebug.h>

#define	LASTUNSIGNED	((u_int)0-1)
#define	dprintf	if (boothowto & RB_DEBUG) printf

/*
 * XDR an array of arbitrary elements
 * *addrp is a pointer to the array, *sizep is the number of elements.
 * If addrp is NULL, then user is reminded that memory is needed
 *
 * elsize is the size (in bytes) of each element, and elproc is the
 * xdr procedure to call to handle each element of the array.
 */
bool_t
xdr_array(xdrs, addrp, sizep, maxsize, elsize, elproc)
	register XDR *xdrs;
	caddr_t *addrp;		/* array pointer */
	u_int *sizep;		/* number of elements */
	u_int maxsize;		/* max numberof elements */
	u_int elsize;		/* size in bytes of each element */
	xdrproc_t elproc;	/* xdr routine to handle each element */
{
	register u_int i;
	register caddr_t target = *addrp;
	register u_int c;  /* the actual element count */
	register bool_t stat = TRUE;
	register u_int nodesize;

	/* like strings, arrays are really counted arrays */
	if (!xdr_u_int(xdrs, sizep))
		return (FALSE);
	c = *sizep;
	if ((c > maxsize) && (xdrs->x_op != XDR_FREE))
		return (FALSE);
	nodesize = c * elsize;

	/*
	 * if we are deserializing, we may need to allocate an array.
	 * We also save time by checking for a null array if we are freeing.
	 */
	if (target == NULL)
		switch (xdrs->x_op) {
		case XDR_DECODE:
			if (c == 0)
				return (TRUE);
			if (*addrp == NULL) {
				dprintf("xdr_array: your array pointer must "
				    "point at allocated space of %d bytes.\n",
				    nodesize);
				return (FALSE);
			}
			bzero(target, nodesize); /* zero what we need */
			break;
		case XDR_FREE:
			return (TRUE);
	}

	/*
	 * now we xdr each element of array
	 */
	for (i = 0; (i < c) && stat; i++) {
		stat = (*elproc)(xdrs, target, LASTUNSIGNED);
		target += elsize;
	}

	return (stat);
}

/*
 * xdr_vector():
 *
 * XDR a fixed length array. Unlike variable-length arrays,
 * the storage of fixed length arrays is static and unfreeable.
 * > basep: base of the array
 * > size: size of the array
 * > elemsize: size of each element
 * > xdr_elem: routine to XDR each element
 */
bool_t
xdr_vector(xdrs, basep, nelem, elemsize, xdr_elem)
	register XDR *xdrs;
	register char *basep;
	register u_int nelem;
	register u_int elemsize;
	register xdrproc_t xdr_elem;
{
	register u_int i;
	register char *elptr;

	elptr = basep;
	for (i = 0; i < nelem; i++) {
		if (! (*xdr_elem)(xdrs, elptr, LASTUNSIGNED)) {
			return (FALSE);
		}
		elptr += elemsize;
	}
	return (TRUE);
}
