/*
 * Copyright (C) 1987, 1999 Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)xdr_refer.c	1.6	99/02/23 SMI" /* from SunOS 4.1 */

/*
 * xdr_reference.c, Generic XDR routines implementation.
 *
 * Modified for use by the boot program.
 *
 * These are the "non-trivial" xdr primitives used to serialize and de-serialize
 * "pointers".  See xdr.h for more info on the interface to xdr.
 */

#include <sys/param.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <sys/promif.h>
#include <sys/salib.h>
#include <sys/bootdebug.h>

#define	LASTUNSIGNED	((u_int)0-1)
#define	dprintf	if (boothowto & RB_DEBUG) printf

/*
 * XDR an indirect pointer
 * xdr_reference is for recursively translating a structure that is
 * referenced by a pointer inside the structure that is currently being
 * translated.  pp references a pointer to storage. If *pp is null
 * the user is slapped.
 * size is the sizeof the referneced structure.
 * proc is the routine to handle the referenced structure.
 */
bool_t
xdr_reference(xdrs, pp, size, proc)
	register XDR *xdrs;
	caddr_t *pp;		/* the pointer to work on */
	u_int size;		/* size of the object pointed to */
	xdrproc_t proc;		/* xdr routine to handle the object */
{
	register caddr_t loc = *pp;
	register bool_t stat;

	if (loc == NULL)
		switch (xdrs->x_op) {
		case XDR_FREE:
			return (TRUE);

		case XDR_DECODE:
			if (*pp == NULL) {
				dprintf("xdr_reference: pointer must point to "
				    "%d bytes of allocated space.\n", size);
				return (FALSE);
			}
			bzero(loc, size);
			break;
	}
	stat = (*proc)(xdrs, loc, LASTUNSIGNED);

	return (stat);
}

/*
 * xdr_pointer():
 *
 * XDR a pointer to a possibly recursive data structure. This
 * differs with xdr_reference in that it can serialize/deserialiaze
 * trees correctly.
 *
 *  What's sent is actually a union:
 *
 *  union object_pointer switch (boolean b) {
 *  case TRUE: object_data data;
 *  case FALSE: void nothing;
 *  }
 *
 * > objpp: Pointer to the pointer to the object.
 * > obj_size: size of the object.
 * > xdr_obj: routine to XDR an object.
 *
 */
bool_t
xdr_pointer(xdrs, objpp, obj_size, xdr_obj)
	register XDR *xdrs;
	char **objpp;
	u_int obj_size;
	xdrproc_t xdr_obj;
{

	bool_t more_data;

	more_data = (*objpp != NULL);
	if (! xdr_bool(xdrs, &more_data)) {
		return (FALSE);
	}
	if (! more_data) {
		*objpp = NULL;
		return (TRUE);
	}
	return (xdr_reference(xdrs, objpp, obj_size, xdr_obj));
}
