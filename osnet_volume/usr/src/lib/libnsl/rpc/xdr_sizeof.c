/*
 * Copyright 1990, 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * General purpose routine to see how much space something will use
 * when serialized using XDR.
 */

#pragma ident	"@(#)xdr_sizeof.c	1.8	97/12/17 SMI"

#include <rpc/types.h>
#include <rpc/xdr.h>
#include <sys/types.h>
#include <rpc/trace.h>
#include <stdlib.h>

/* ARGSUSED */
static bool_t
x_putlong(XDR *xdrs, long *ip)
{
	trace1(TR_x_putlong, 0);

	xdrs->x_handy += BYTES_PER_XDR_UNIT;
	trace1(TR_x_putlong, 1);
	return (TRUE);
}

static bool_t
x_putint32_t(XDR *xdrs, int32_t *ip)
{
	trace1(TR_x_putint32_t, 0);

	xdrs->x_handy += BYTES_PER_XDR_UNIT;
	trace1(TR_x_putint32_t, 1);
	return (TRUE);
}

/* ARGSUSED */
static bool_t
x_putbytes(XDR *xdrs, char *bp, int len)
{
	trace2(TR_x_putbytes, 0, len);
	xdrs->x_handy += len;
	trace2(TR_x_putbytes, 1, len);

	return (TRUE);
}

static u_int
x_getpostn(XDR *xdrs)
{
	trace1(TR_x_getpostn, 0);
	trace1(TR_x_getpostn, 1);
	return (xdrs->x_handy);
}

/* ARGSUSED */
static bool_t
x_setpostn(XDR *xdrs, u_int pos)
{
	/* This is not allowed */
	trace2(TR_x_setpostn, 0, pos);
	trace2(TR_x_setpostn, 1, pos);
	return (FALSE);
}

static rpc_inline_t *
x_inline(XDR *xdrs, int len)
{
	trace2(TR_x_inline, 0, len);
	if (len == 0) {
		trace2(TR_x_inline, 1, len);
		return (NULL);
	}
	if (xdrs->x_op != XDR_ENCODE) {
		trace2(TR_x_inline, 1, len);
		return (NULL);
	}
	if (len < (int) xdrs->x_base) {
		/* x_private was already allocated */
		xdrs->x_handy += len;
		trace2(TR_x_inline, 1, len);
		return ((rpc_inline_t *) xdrs->x_private);
	} else {
		/* Free the earlier space and allocate new area */
		if (xdrs->x_private)
			free(xdrs->x_private);
		if ((xdrs->x_private = (caddr_t) malloc(len)) == NULL) {
			xdrs->x_base = 0;
			trace2(TR_x_inline, 1, len);
			return (NULL);
		}
		xdrs->x_base = (caddr_t) len;
		xdrs->x_handy += len;
		trace2(TR_x_inline, 1, len);
		return ((rpc_inline_t *) xdrs->x_private);
	}
}

static
harmless()
{
	/* Always return FALSE/NULL, as the case may be */
	trace1(TR_harmless, 0);
	trace1(TR_harmless, 1);
	return (0);
}

static void
x_destroy(XDR *xdrs)
{
	trace1(TR_x_destroy, 0);
	xdrs->x_handy = 0;
	xdrs->x_base = 0;
	if (xdrs->x_private) {
		free(xdrs->x_private);
		xdrs->x_private = NULL;
	}
	trace1(TR_x_destroy, 1);
}

unsigned int
xdr_sizeof(xdrproc_t func, void *data)
{
	XDR x;
	struct xdr_ops ops;
	bool_t stat;
	/* to stop ANSI-C compiler from complaining */
	typedef  bool_t (* dummyfunc1)(XDR *, long *);
	typedef  bool_t (* dummyfunc2)(XDR *, caddr_t, int);
	typedef  bool_t (* dummyfunc3)(XDR *, int32_t *);

	trace1(TR_xdr_sizeof, 0);
	ops.x_putlong = x_putlong;
	ops.x_getlong =  (dummyfunc1) harmless;
	ops.x_putbytes = x_putbytes;
	ops.x_inline = x_inline;
	ops.x_getpostn = x_getpostn;
	ops.x_setpostn = x_setpostn;
	ops.x_destroy = x_destroy;
#if defined(_LP64)
	ops.x_getint32 = (dummyfunc3) harmless;
	ops.x_putint32 = x_putint32_t;
#endif

	/* the other harmless ones */
	ops.x_getbytes = (dummyfunc2) harmless;

	x.x_op = XDR_ENCODE;
	x.x_ops = &ops;
	x.x_handy = 0;
	x.x_private = (caddr_t) NULL;
	x.x_base = (caddr_t) 0;

	stat = func(&x, data);
	if (x.x_private)
		free(x.x_private);
	trace1(TR_xdr_sizeof, 1);
	return (stat == TRUE ? (unsigned int) x.x_handy: 0);
}
