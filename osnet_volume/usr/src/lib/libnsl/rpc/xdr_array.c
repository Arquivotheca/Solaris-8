
/*
 * Copyright (c) 1986 - 1991 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)xdr_array.c	1.11	97/04/29 SMI"

/*
 * xdr_array.c, Generic XDR routines impelmentation.
 *
 * These are the "non-trivial" xdr primitives used to serialize and de-serialize
 * arrays.  See xdr.h for more info on the interface to xdr.
 */

#include <sys/types.h>
#include <rpc/trace.h>
#ifdef KERNEL
#include <sys/param.h>
#include <sys/systm.h>
#else
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#endif

#include <rpc/types.h>
#include <rpc/xdr.h>
#include <memory.h>

#define	LASTUNSIGNED	((u_int)0-1)

char mem_err_msg_arr[] = "xdr_array: out of memory";
/*
 * XDR an array of arbitrary elements
 * *addrp is a pointer to the array, *sizep is the number of elements.
 * If *addrp is NULL (*sizep * elsize) bytes are allocated.
 * elsize is the size (in bytes) of each element, and elproc is the
 * xdr procedure to call to handle each element of the array.
 */
bool_t
xdr_array(XDR *xdrs, caddr_t *addrp, u_int *sizep, u_int maxsize,
	u_int elsize, xdrproc_t elproc)
{
	register u_int i;
	register caddr_t target = *addrp;
	register u_int c;  /* the actual element count */
	register bool_t stat = TRUE;
	register u_int nodesize;

	trace3(TR_xdr_array, 0, maxsize, elsize);
	/* like strings, arrays are really counted arrays */
	if (! xdr_u_int(xdrs, sizep)) {
#ifdef KERNEL
		printf("xdr_array: size FAILED\n");
#endif
		trace1(TR_xdr_array, 1);
		return (FALSE);
	}
	c = *sizep;
	if ((c > maxsize) && (xdrs->x_op != XDR_FREE)) {
#ifdef KERNEL
		printf("xdr_array: bad size FAILED\n");
#endif
		trace1(TR_xdr_array, 1);
		return (FALSE);
	}
	nodesize = c * elsize;

	/*
	 * if we are deserializing, we may need to allocate an array.
	 * We also save time by checking for a null array if we are freeing.
	 */
	if (target == NULL)
		switch (xdrs->x_op) {
		case XDR_DECODE:
			if (c == 0) {
				trace1(TR_xdr_array, 1);
				return (TRUE);
			}
			*addrp = target = (caddr_t)mem_alloc(nodesize);
#ifndef KERNEL
			if (target == NULL) {
				(void) syslog(LOG_ERR, mem_err_msg_arr);
				trace1(TR_xdr_array, 1);
				return (FALSE);
			}
#endif
			(void) memset(target, 0, nodesize);
			break;

		case XDR_FREE:
			trace1(TR_xdr_array, 1);
			return (TRUE);
	}

	/*
	 * now we xdr each element of array
	 */
	for (i = 0; (i < c) && stat; i++) {
		stat = (*elproc)(xdrs, target);
		target += elsize;
	}

	/*
	 * the array may need freeing
	 */
	if (xdrs->x_op == XDR_FREE) {
		mem_free(*addrp, nodesize);
		*addrp = NULL;
	}
	trace1(TR_xdr_array, 1);
	return (stat);
}

#ifndef KERNEL
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
xdr_vector(XDR *xdrs, char *basep, u_int nelem,
	u_int elemsize, xdrproc_t xdr_elem)
{
	register u_int i;
	register char *elptr;

	trace3(TR_xdr_vector, 0, nelem, elemsize);
	elptr = basep;
	for (i = 0; i < nelem; i++) {
		if (! (*xdr_elem)(xdrs, elptr, LASTUNSIGNED)) {
			trace1(TR_xdr_vector, 1);
			return (FALSE);
		}
		elptr += elemsize;
	}
	trace1(TR_xdr_vector, 1);
	return (TRUE);
}
#endif /* !KERNEL */
