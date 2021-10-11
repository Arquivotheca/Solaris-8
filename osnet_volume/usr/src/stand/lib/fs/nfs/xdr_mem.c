/*
 * Copyright (C) 1984-1999, Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)xdr_mem.c	1.9	99/02/23 SMI" /* from SunOS 4.1 1.23 90/03/30 */

/*
 * xdr_mem.h, XDR implementation using memory buffers.
 *
 * Modified for the use of the boot program.
 *
 * If you have some data to be interpreted as external data representation
 * or to be converted to external data representation in a memory buffer,
 * then this is the package for you.
 */

#include <sys/types.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <netinet/in.h>
#include <sys/salib.h>

static struct xdr_ops *xdrmem_ops(void);

/*
 * The procedure xdrmem_create initializes a stream descriptor for a
 * memory buffer.
 */
void
xdrmem_create(XDR *xdrs, caddr_t addr, u_int size, enum xdr_op op)
{
	xdrs->x_op = op;
	xdrs->x_ops = xdrmem_ops();
	xdrs->x_private = xdrs->x_base = addr;
	xdrs->x_handy = size;
	xdrs->x_public = NULL;
}

/* ARGSUSED */
static void
xdrmem_destroy(XDR *xdrs)
{
}

static bool_t
xdrmem_getint32(XDR *xdrs, int32_t *int32p)
{
	if ((xdrs->x_handy -= sizeof (int32_t)) < 0)
		return (FALSE);
	/* LINTED pointer alignment */
	*int32p = (int32_t)ntohl((uint32_t)(*((int32_t *)(xdrs->x_private))));
	xdrs->x_private += sizeof (int32_t);
	return (TRUE);
}

static bool_t
xdrmem_putint32(XDR *xdrs, int32_t *int32p)
{
	if ((xdrs->x_handy -= sizeof (int32_t)) < 0)
		return (FALSE);
	/* LINTED pointer alignment */
	*(int32_t *)xdrs->x_private = (int32_t)htonl((uint32_t)(*int32p));
	xdrs->x_private += sizeof (int32_t);
	return (TRUE);
}

static bool_t
xdrmem_getbytes(XDR *xdrs, caddr_t addr, int len)
{
	if ((xdrs->x_handy -= len) < 0)
		return (FALSE);
	bcopy(xdrs->x_private, addr, len);
	xdrs->x_private += len;
	return (TRUE);
}

static bool_t
xdrmem_putbytes(XDR *xdrs, caddr_t addr, int len)
{
	if ((xdrs->x_handy -= len) < 0)
		return (FALSE);
	bcopy(addr, xdrs->x_private, len);
	xdrs->x_private += len;
	return (TRUE);
}

static u_int
xdrmem_getpos(XDR *xdrs)
{
	return ((uintptr_t)xdrs->x_private - (uintptr_t)xdrs->x_base);
}

static bool_t
xdrmem_setpos(XDR *xdrs, u_int pos)
{
	caddr_t newaddr = xdrs->x_base + pos;
	caddr_t lastaddr = xdrs->x_private + xdrs->x_handy;
	ptrdiff_t diff;

	if (newaddr > lastaddr)
		return (FALSE);
	xdrs->x_private = newaddr;
	diff = lastaddr - newaddr;
	xdrs->x_handy = (int)diff;
	return (TRUE);
}

static rpc_inline_t *
xdrmem_inline(XDR *xdrs, int len)
{
	int32_t *buf = NULL;

	if (xdrs->x_handy >= len) {
		xdrs->x_handy -= len;
		/* LINTED pointer alignment */
		buf = (rpc_inline_t *)xdrs->x_private;
		xdrs->x_private += len;
	}
	return (buf);
}

/*ARGSUSED*/
static bool_t
xdrmem_control(XDR *xdrs, int request, void *info)
{
	/* not used in stand, just a filler for the xdr_ops */
	return (FALSE);
}

static struct xdr_ops *
xdrmem_ops(void)
{
	static struct xdr_ops ops;

	if (ops.x_getint32 == NULL) {
		ops.x_getbytes = xdrmem_getbytes;
		ops.x_putbytes = xdrmem_putbytes;
		ops.x_getpostn = xdrmem_getpos;
		ops.x_setpostn = xdrmem_setpos;
		ops.x_inline = xdrmem_inline;
		ops.x_destroy = xdrmem_destroy;
		ops.x_control = xdrmem_control;
		ops.x_getint32 = xdrmem_getint32;
		ops.x_putint32 = xdrmem_putint32;
	}
	return (&ops);
}
