/*
 * Copyright (c) 1992 - 1993 by Sun Microsystems, Inc.
 */

#pragma ident "@(#)FN_ref_addr_c.cc	1.3 97/10/16 SMI"


#include <xfn/FN_ref_addr.hh>

extern "C"
FN_ref_addr_t *
fn_ref_addr_create(const FN_identifier_t *type, size_t len, const void *data)
{
	FN_identifier addr_type(*type);
	return ((FN_ref_addr_t *)new FN_ref_addr(addr_type, len, data));
}

extern "C"
void
fn_ref_addr_destroy(FN_ref_addr_t *p)
{
	delete (FN_ref_addr *)p;
}

extern "C"
FN_ref_addr_t *
fn_ref_addr_copy(const FN_ref_addr_t *p)
{
	return ((FN_ref_addr_t *)
		new FN_ref_addr(*((const FN_ref_addr *)p)));
}

extern "C"
FN_ref_addr_t *
fn_ref_addr_assign(FN_ref_addr_t *p1, const FN_ref_addr_t *p2)
{
	return ((FN_ref_addr_t *)
	    &(*((FN_ref_addr *)p1) = *((const FN_ref_addr *)p2)));
}

extern "C"
const FN_identifier_t *
fn_ref_addr_type(const FN_ref_addr_t *p)
{
	const FN_identifier *ident = ((const FN_ref_addr *)p)->type();
	if (ident)
	    return (&(ident->info));
	else
	    return (0);
}

extern "C"
size_t
fn_ref_addr_length(const FN_ref_addr_t *p)
{
	return (((const FN_ref_addr *)p)->length());
}

extern "C"
const void *
fn_ref_addr_data(const FN_ref_addr_t *p)
{
	return (((const FN_ref_addr *)p)->data());
}

extern "C"
FN_string_t *
fn_ref_addr_description(const FN_ref_addr_t *p,
    unsigned detail, unsigned *more_detail)
{
	FN_string *desc =
	    ((const FN_ref_addr *)p)-> description(detail, more_detail);
	return ((FN_string_t *)desc);
}
