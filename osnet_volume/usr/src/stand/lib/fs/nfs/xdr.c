/*
 * Copyright (C) 1986-1999, Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)xdr.c	1.14	99/02/23 SMI" /* from SunOS 4.1 1.41 90/03/30 */

/*
 * xdr.c, Generic XDR routines implementation.
 *
 * Modified for use in the boot program. NOTE: No memory allocation is
 * done in this version of the boot program. It is assumed that the
 * addresses passed in for storing the results in are "nodesize", and
 * are valid. If they are null, then the routine will report this and
 * fail. If the ptrs contain jibberish... You're in trouble in either
 * case.
 *
 * These are the "generic" xdr routines used to serialize and de-serialize
 * most common data items.  See xdr.h for more info on the interface to
 * xdr.
 */

#include <sys/types.h>
#include <sys/salib.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <sys/promif.h>
#include <sys/bootdebug.h>

/*
 * constants specific to the xdr "protocol"
 */
#define	XDR_FALSE	((int32_t)0)
#define	XDR_TRUE	((int32_t)1)
#define	LASTUNSIGNED	((u_int)0-1)

#define	dprintf	if (boothowto & RB_DEBUG) printf

/*
 * for unit alignment
 */
static char xdr_zero[BYTES_PER_XDR_UNIT] = { 0, 0, 0, 0 };

/*
 * Free a data structure using XDR
 * Not a filter, but a convenient utility nonetheless
 */
void
xdr_free(xdrproc_t proc, char *objp)
{
	XDR x;

	x.x_op = XDR_FREE;
	(*proc)(&x, objp);
}

/*
 * XDR nothing
 */
bool_t
xdr_void(void)
{
	return (TRUE);
}

/*
 * XDR integers
 */
bool_t
xdr_int(XDR *xdrs, int *ip)
{
	if (xdrs->x_op == XDR_ENCODE)
		return (XDR_PUTINT32(xdrs, ip));

	if (xdrs->x_op == XDR_DECODE)
		return (XDR_GETINT32(xdrs, ip));

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);

	return (FALSE);
}

/*
 * XDR unsigned integers
 */
bool_t
xdr_u_int(XDR *xdrs, u_int *up)
{
	if (xdrs->x_op == XDR_ENCODE)
		return (XDR_PUTINT32(xdrs, (int32_t *)up));

	if (xdrs->x_op == XDR_DECODE)
		return (XDR_GETINT32(xdrs, (int32_t *)up));

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);

	return (FALSE);
}

/*
 * XDR short integers
 */
bool_t
xdr_short(XDR *xdrs, short *sp)
{
	int32_t l;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		l = (int32_t)*sp;
		return (XDR_PUTINT32(xdrs, &l));

	case XDR_DECODE:
		if (!XDR_GETINT32(xdrs, &l))
			return (FALSE);
		*sp = (short)l;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}

/*
 * XDR unsigned short integers
 */
bool_t
xdr_u_short(XDR *xdrs, u_short *usp)
{
	uint32_t l;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		l = (uint32_t)*usp;
		return (XDR_PUTINT32(xdrs, (int32_t *)&l));

	case XDR_DECODE:
		if (!XDR_GETINT32(xdrs, (int32_t *)&l)) {
#ifdef	DEBUG
			printf("xdr_u_short: decode FAILED\n");
#endif	/* DEBUG */
			return (FALSE);
		}
		*usp = (u_short)l;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}


/*
 * XDR a char
 */
bool_t
xdr_char(XDR *xdrs, char *cp)
{
	int i;

	i = (*cp);
	if (!xdr_int(xdrs, &i))
		return (FALSE);
	*cp = i;
	return (TRUE);
}

/*
 * XDR an unsigned char
 */
bool_t
xdr_u_char(XDR *xdrs, char *cp)
{
	u_int U;

	U = (*cp);
	if (!xdr_u_int(xdrs, &U))
		return (FALSE);
	*cp = (char)U;
	return (TRUE);
}

/*
 * XDR booleans
 */
bool_t
xdr_bool(XDR *xdrs, bool_t *bp)
{
	int32_t i32b;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		i32b = *bp ? XDR_TRUE : XDR_FALSE;
		return (XDR_PUTINT32(xdrs, &i32b));

	case XDR_DECODE:
		if (!XDR_GETINT32(xdrs, &i32b)) {
#ifdef	DEBUG
			printf("xdr_bool: decode FAILED\n");
#endif	/* DEBUG */
			return (FALSE);
		}
		*bp = (i32b == XDR_FALSE) ? FALSE : TRUE;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}

/*
 * XDR enumerations
 */
bool_t
xdr_enum(XDR *xdrs, enum_t *ep)
{
#ifndef lint
	enum sizecheck { SIZEVAL };	/* used to find the size of an enum */

	/*
	 * enums are treated as ints
	 */
	if (sizeof (enum sizecheck) == sizeof (int32_t)) {
		return (xdr_int(xdrs, (int32_t *)ep));
	} else if (sizeof (enum sizecheck) == sizeof (short)) {
		return (xdr_short(xdrs, (short *)ep));
	} else {
		return (FALSE);
	}
#else
	(void) (xdr_short(xdrs, (short *)ep));
	return (xdr_int(xdrs, (int32_t *)ep));
#endif
}

/*
 * XDR opaque data
 * Allows the specification of a fixed size sequence of opaque bytes.
 * cp points to the opaque object and cnt gives the byte length.
 */
bool_t
xdr_opaque(XDR *xdrs, caddr_t cp, const u_int cnt)
{
	u_int rndup;
	static crud[BYTES_PER_XDR_UNIT];

	/*
	 * if no data we are done
	 */
	if (cnt == 0) {
#ifdef DEBUG
		printf("xdr_opaque: cnt is zero, no work.\n");
#endif /* DEBUG */
		return (TRUE);
	}

	/*
	 * round byte count to full xdr units
	 */
	rndup = cnt % BYTES_PER_XDR_UNIT;
	if (rndup != 0)
		rndup = BYTES_PER_XDR_UNIT - rndup;

	if (xdrs->x_op == XDR_DECODE) {
		if (!XDR_GETBYTES(xdrs, cp, cnt))
			return (FALSE);
		if (rndup == 0)
			return (TRUE);
		return (XDR_GETBYTES(xdrs, (caddr_t)crud, rndup));
	}

	if (xdrs->x_op == XDR_ENCODE) {
		if (!XDR_PUTBYTES(xdrs, cp, cnt))
			return (FALSE);
		if (rndup == 0)
			return (TRUE);
		return (XDR_PUTBYTES(xdrs, xdr_zero, rndup));
	}

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);
	return (FALSE);
}

/*
 * XDR counted bytes
 * *cpp is a pointer to the bytes, *sizep is the count.
 * If *cpp is NULL, function fails!
 */
bool_t
xdr_bytes(XDR *xdrs, char **cpp, u_int *sizep, const u_int maxsize)
{
	char *sp = *cpp;  /* sp is the actual string pointer */
	u_int nodesize;

	/*
	 * first deal with the length since xdr bytes are counted
	 */
	if (!xdr_u_int(xdrs, sizep)) {
#ifdef	DEBUG
		printf("xdr_bytes: size FAILED\n");
#endif	/* DEBUG */
		return (FALSE);
	}
	nodesize = *sizep;
	if ((nodesize > maxsize) && (xdrs->x_op != XDR_FREE))
		return (FALSE);

	/*
	 * now deal with the actual bytes
	 */
	switch (xdrs->x_op) {
	case XDR_DECODE:
		if (nodesize == 0)
			return (TRUE);
		if (sp == NULL) {
#ifdef	DEBUG
			dprintf("xdr_bytes: You have to allocate %d bytes "
			    "for the string you are passing in.\n", nodesize);
#endif	/* DEBUG */
			return (FALSE);
		}
		/*FALLTHROUGH*/
	case XDR_ENCODE:
		return (xdr_opaque(xdrs, sp, nodesize));

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}

/*
 * XDR a descriminated union
 * Support routine for discriminated unions.
 * You create an array of xdrdiscrim structures, terminated with
 * an entry with a null procedure pointer.  The routine gets
 * the discriminant value and then searches the array of xdrdiscrims
 * looking for that value.  It calls the procedure given in the xdrdiscrim
 * to handle the discriminant.  If there is no specific routine a default
 * routine may be called.
 * If there is no specific or default routine an error is returned.
 */
bool_t
xdr_union(XDR *xdrs, enum_t *dscmp, char *unp,
	const struct xdr_discrim *choices, const xdrproc_t dfault)
{
	enum_t dscm;

	/*
	 * we deal with the discriminator;  it's an enum
	 */
	if (!xdr_enum(xdrs, dscmp)) {
#ifdef	DEBUG
		printf("xdr_enum: dscmp FAILED\n");
#endif	/* DEBUG */
		return (FALSE);
	}
	dscm = *dscmp;

	/*
	 * search choices for a value that matches the discriminator.
	 * if we find one, execute the xdr routine for that value.
	 */
	for (; choices->proc != NULL_xdrproc_t; choices++) {
		if (choices->value == dscm)
			return ((*(choices->proc))(xdrs, unp, LASTUNSIGNED));
	}

	/*
	 * no match - execute the default xdr routine if there is one
	 */
	return ((dfault == NULL_xdrproc_t) ? FALSE :
	    (*dfault)(xdrs, unp, LASTUNSIGNED));
}


/*
 * Non-portable xdr primitives.
 * Care should be taken when moving these routines to new architectures.
 */


/*
 * XDR null terminated ASCII strings
 * xdr_string deals with "C strings" - arrays of bytes that are
 * terminated by a NULL character.  The parameter cpp references a
 * pointer to storage; If the pointer is null, then the necessary
 * storage is allocated.  The last parameter is the max allowed length
 * of the string as specified by a protocol.
 */
bool_t
xdr_string(XDR *xdrs, char **cpp, const u_int maxsize)
{
	char *sp = *cpp;  /* sp is the actual string pointer */
	u_int size;
	u_int nodesize;

	/*
	 * first deal with the length since xdr strings are counted-strings
	 */
	switch (xdrs->x_op) {
	case XDR_FREE:
		if (sp == NULL)
			return (TRUE);	/* already free */
		/* FALLTHROUGH */
	case XDR_ENCODE:
		size = (sp != NULL) ? strlen(sp) : 0;
		break;
	}
	if (!xdr_u_int(xdrs, &size)) {
#ifdef	DEBUG
		printf("xdr_string: size FAILED\n");
#endif	/* DEBUG */
		return (FALSE);
	}
	if (size > maxsize)
		return (FALSE);
	nodesize = size + 1;

	/*
	 * now deal with the actual bytes
	 */
	switch (xdrs->x_op) {
	case XDR_DECODE:
		if (nodesize == 0)
			return (TRUE);
		if (sp == NULL) {
#ifdef	DEBUG
			dprintf("xdr_string: You have to allocate %d bytes for "
			    "the string you are passing in.\n", nodesize);
#endif	/* DEBUG */
			return (FALSE);
		}
		/*FALLTHROUGH*/
	case XDR_ENCODE:
		return (xdr_opaque(xdrs, sp, size));

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}

/*
 * Wrapper for xdr_string that can be called directly from
 * routines like clnt_call.
 */
bool_t
xdr_wrapstring(XDR *xdrs, char **cpp)
{
	if (xdr_string(xdrs, cpp, LASTUNSIGNED))
		return (TRUE);
	return (FALSE);
}
