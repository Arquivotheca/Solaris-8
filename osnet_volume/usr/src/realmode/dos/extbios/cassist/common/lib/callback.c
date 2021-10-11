/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Driver callback interface:
 *
 *   This file implements the client (driver) side of the ".bef" callback
 *   interface.  Five routines are provided:
 *
 *       node_op()	- Establish & dispose of device tree nodes
 *       set_res()	- Set resource usage for a given device node
 *       rel_res()	- Release a resource for a given node
 *       get_res()	- Get resource usage for a given node
 *       set_prop()	- Set a property for the current node
 *       get_prop()	- Get a property from the current node (or options)
 *       set_root_prop()- Set a property for the / node
 *       get_root_prop()- Get a property from the / node
 *	 mem_adjust()	- Adjust BEF's current memory segment
 *
 *   All of these routines are, in fact, implemented on the server side
 *   of the interface (i.e, in the program that loaded the driver that is
 *   calling into this module).  All we do here is to call back into the
 *   server code via the transfer vector saved by the C startup routine.
 */

#ident "<@(#)callback.c	1.8	98/03/12	SMI>"
#include <befext.h>
#include <stdarg.h>

extern int befunc;
extern struct bef_interface far *befvec;

int
node_op(int op)
{
	return ((*befvec->node)(op));
}

int
get_res(char _far *nam, DWORD _far *buf, DWORD _far *len)
{
	return ((*befvec->resource)(RES_GET, nam, buf, len));
}

int
set_res(char _far *nam, DWORD _far *buf, DWORD _far *len, int flag)
{
	return ((*befvec->resource)((RES_SET|flag), nam, buf, len));
}

int
rel_res(char _far *nam, DWORD _far *buf, DWORD _far *len)
{
	return ((*befvec->resource)(RES_REL, nam, buf, len));
}

int
get_prop(char _far *nam, char _far * _far* val, int _far *len)
{
	return ((*befvec->prop)(PROP_GET, nam, val, len, 0));
}

int
set_prop(char _far *nam, char _far * _far *val, int _far *len, int bin)
{
	return ((*befvec->prop)(PROP_SET, nam, val, len, bin));
}

int
get_root_prop(char _far *nam, char _far * _far* val, int _far *len)
{
	return ((*befvec->prop)(PROP_GET_ROOT, nam, val, len, 0));
}

int
set_root_prop(char _far *nam, char _far * _far *val, int _far *len, int bin)
{
	return ((*befvec->prop)(PROP_SET_ROOT, nam, val, len, bin));
}

int
putc_(int c)
{
	return ((*befvec->putc)(c));
}
