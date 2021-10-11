/*
 * Copyright (c) 1984-1991,1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)xdr_mem.c	1.21	99/10/05 SMI"

#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)xdr_mem.c	1.21	99/10/05 SMI";
#endif

/*
 * xdr_mem.h, XDR implementation using memory buffers.
 *
 * If you have some data to be interpreted as external data representation
 * or to be converted to external data representation in a memory buffer,
 * then this is the package for you.
 *
 */

#ifdef KERNEL
#include <sys/param.h>
#endif

#include "rpc_mt.h"
#include <sys/types.h>
#include <rpc/trace.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <memory.h>
#include <inttypes.h>
#include <syslog.h>

static struct xdr_ops *xdrmem_ops(void);

/*
 * Meaning of the private areas of the xdr struct for xdr_mem
 * 	x_base : Base from where the xdr stream starts
 * 	x_private : The current position of the stream.
 * 	x_handy : The size of the stream buffer.
 */

/*
 * The procedure xdrmem_create initializes a stream descriptor for a
 * memory buffer.
 */
void
xdrmem_create(XDR *xdrs, caddr_t addr, uint_t size, enum xdr_op op)
{
	trace2(TR_xdrmem_create, 0, size);
	xdrs->x_op = op;
	xdrs->x_ops = xdrmem_ops();
	xdrs->x_private = xdrs->x_base = addr;
	xdrs->x_handy = size;
	trace2(TR_xdrmem_create, 1, size);
}

static void
xdrmem_destroy(XDR *xdrs)
{
	trace1(TR_xdrmem_destroy, 0);
	trace1(TR_xdrmem_destroy, 1);
}

static bool_t
xdrmem_getlong(XDR *xdrs, long *lp)
{
	int tmp;

	trace1(TR_xdrmem_getlong, 0);
	if ((tmp = (xdrs->x_handy - (int)sizeof (int32_t))) < 0) {
		syslog(LOG_WARNING,
			"xdrmem_getlong: Incoming data too large, ",
			"Can't decode args (base=%p, private=%p, handy=%d)",
			xdrs->x_base, xdrs->x_private, xdrs->x_handy);
		xdrs->x_private += xdrs->x_handy;
		xdrs->x_handy = 0;
		trace1(TR_xdrmem_getlong, 1);
		return (FALSE);
	}
	xdrs->x_handy = tmp;
	*lp = (int32_t)ntohl((uint32_t)(*((int32_t *)(xdrs->x_private))));
	xdrs->x_private += sizeof (int32_t);
	trace1(TR_xdrmem_getlong, 1);
	return (TRUE);
}

static bool_t
xdrmem_putlong(XDR *xdrs, long *lp)
{
	int tmp;

	trace1(TR_xdrmem_putlong, 0);
#if defined(_LP64)
	if ((*lp > INT32_MAX) || (*lp < INT32_MIN)) {
		return (FALSE);
	}
#endif

	if ((tmp = (xdrs->x_handy - (int)sizeof (int32_t))) < 0) {
		syslog(LOG_WARNING,
			"xdrmem_putlong: Outgoing data too large, ",
			"Can't encode args (base=%p, private=%p, handy=%d)",
			xdrs->x_base, xdrs->x_private, xdrs->x_handy);
		xdrs->x_private += xdrs->x_handy;
		xdrs->x_handy = 0;
		trace1(TR_xdrmem_putlong, 1);
		return (FALSE);
	}
	xdrs->x_handy = tmp;
	*(int32_t *)xdrs->x_private = (int32_t)htonl((uint32_t)(*lp));
	xdrs->x_private += sizeof (int32_t);
	trace1(TR_xdrmem_putlong, 1);
	return (TRUE);
}

#if defined(_LP64)

static bool_t
xdrmem_getint32(XDR *xdrs, int32_t *ip)
{
	int tmp;

	trace1(TR_xdrmem_getint32, 0);
	if ((tmp = (xdrs->x_handy - (int)sizeof (int32_t))) < 0) {
		syslog(LOG_WARNING,
			"xdrmem_getint32: Incoming data too large, ",
			"Can't decode args (base=%p, private=%p, handy=%d)",
			xdrs->x_base, xdrs->x_private, xdrs->x_handy);
		xdrs->x_private += xdrs->x_handy;
		xdrs->x_handy = 0;
		trace1(TR_xdrmem_putlong, 1);
		return (FALSE);
	}
	xdrs->x_handy = tmp;
	*ip = (int32_t)ntohl((uint32_t)(*((int32_t *)(xdrs->x_private))));
	xdrs->x_private += sizeof (int32_t);
	trace1(TR_xdrmem_getint32, 1);
	return (TRUE);
}

static bool_t
xdrmem_putint32(XDR *xdrs, int32_t *ip)
{
	int tmp;

	trace1(TR_xdrmem_putint32, 0);
	if ((tmp = (xdrs->x_handy - (int)sizeof (int32_t))) < 0) {
		syslog(LOG_WARNING,
			"xdrmem_putint32: Outgoing data too large, ",
			"Can't encode args (base=%p, private=%p, handy=%d)",
			xdrs->x_base, xdrs->x_private, xdrs->x_handy);
		xdrs->x_private += xdrs->x_handy;
		xdrs->x_handy = 0;
		trace1(TR_xdrmem_putlong, 1);
		return (FALSE);
	}
	xdrs->x_handy = tmp;
	*(int32_t *)xdrs->x_private = (int32_t)htonl((uint32_t)(*ip));
	xdrs->x_private += sizeof (int32_t);
	trace1(TR_xdrmem_putint32, 1);
	return (TRUE);
}

#endif /* _LP64 */

static bool_t
xdrmem_getbytes(XDR *xdrs, caddr_t addr, int len)
{
	int tmp;

	trace2(TR_xdrmem_getbytes, 0, len);
	if ((tmp = (xdrs->x_handy - len)) < 0) {
		syslog(LOG_WARNING,
		"xdrmem_getbytes: Incoming data too large, ",
		"Can't decode args (base=%p, private=%p, handy=%d, length=%d)",
		xdrs->x_base, xdrs->x_private, xdrs->x_handy, len);
		xdrs->x_private += xdrs->x_handy;
		xdrs->x_handy = 0;
		trace1(TR_xdrmem_getbytes, 1);
		return (FALSE);
	}
	xdrs->x_handy = tmp;
	(void) memcpy(addr, xdrs->x_private, len);
	xdrs->x_private += len;
	trace1(TR_xdrmem_getbytes, 1);
	return (TRUE);
}

static bool_t
xdrmem_putbytes(XDR *xdrs, caddr_t addr, int len)
{
	int tmp;

	trace2(TR_xdrmem_putbytes, 0, len);
	if ((tmp = (xdrs->x_handy - len)) < 0) {
		syslog(LOG_WARNING,
		"xdrmem_putbytes: Outgoing data too large, ",
		"Can't encode args (base=%p, private=%p, handy=%d, length=%d)",
		xdrs->x_base, xdrs->x_private, xdrs->x_handy, len);
		xdrs->x_private += xdrs->x_handy;
		xdrs->x_handy = 0;
		trace1(TR_xdrmem_putbytes, 1);
		return (FALSE);
	}
	xdrs->x_handy = tmp;
	(void) memcpy(xdrs->x_private, addr, len);
	xdrs->x_private += len;
	trace1(TR_xdrmem_putbytes, 1);
	return (TRUE);
}

static uint_t
xdrmem_getpos(xdrs)
	register XDR *xdrs;
{
	trace1(TR_xdrmem_getpos, 0);
	trace1(TR_xdrmem_getpos, 1);
	return (uint_t)((uintptr_t)xdrs->x_private - (uintptr_t)xdrs->x_base);
}

static bool_t
xdrmem_setpos(xdrs, pos)
	register XDR *xdrs;
	uint_t pos;
{
	register caddr_t newaddr = xdrs->x_base + pos;
	register caddr_t lastaddr = xdrs->x_private + xdrs->x_handy;

	trace2(TR_xdrmem_setpos, 0, pos);
	if ((long)newaddr > (long)lastaddr) {
		trace1(TR_xdrmem_setpos, 1);
		return (FALSE);
	}
	xdrs->x_private = newaddr;
	xdrs->x_handy = (int)((uintptr_t)lastaddr - (uintptr_t)newaddr);
	trace1(TR_xdrmem_setpos, 1);
	return (TRUE);
}
static rpc_inline_t *
xdrmem_inline(XDR *xdrs, int len)
{
	rpc_inline_t *buf = 0;

	trace2(TR_xdrmem_inline, 0, len);
	if (xdrs->x_handy >= len) {
		xdrs->x_handy -= len;
		buf = (rpc_inline_t *)xdrs->x_private;
		xdrs->x_private += len;
	}
	trace2(TR_xdrmem_inline, 1, len);
	return (buf);
}

static bool_t
xdrmem_control(XDR *xdrs, int request, void *info)
{
	xdr_bytesrec *xptr;

	switch (request) {

	case XDR_GET_BYTES_AVAIL:
		xptr = (xdr_bytesrec *) info;
		xptr->xc_is_last_record = TRUE;
		xptr->xc_num_avail = xdrs->x_handy;
		return (TRUE);
	default:
		return (FALSE);

	}

}

static struct xdr_ops *
xdrmem_ops()
{
	static struct xdr_ops ops;
	extern mutex_t	ops_lock;

/* VARIABLES PROTECTED BY ops_lock: ops */


	trace1(TR_xdrmem_ops, 0);
	mutex_lock(&ops_lock);
	if (ops.x_getlong == NULL) {
		ops.x_getlong = xdrmem_getlong;
		ops.x_putlong = xdrmem_putlong;
		ops.x_getbytes = xdrmem_getbytes;
		ops.x_putbytes = xdrmem_putbytes;
		ops.x_getpostn = xdrmem_getpos;
		ops.x_setpostn = xdrmem_setpos;
		ops.x_inline = xdrmem_inline;
		ops.x_destroy = xdrmem_destroy;
		ops.x_control = xdrmem_control;
#if defined(_LP64)
		ops.x_getint32 = xdrmem_getint32;
		ops.x_putint32 = xdrmem_putint32;
#endif
	}
	mutex_unlock(&ops_lock);
	trace1(TR_xdrmem_ops, 1);
	return (&ops);
}
