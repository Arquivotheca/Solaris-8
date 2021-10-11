/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_ref_c.cc	1.3	96/03/31 SMI"

#include <xfn/FN_ref.hh>

extern "C"
FN_ref_t *
fn_ref_create(const FN_identifier_t *type)
{
	FN_identifier ref_type(*type);
	return ((FN_ref_t *)new FN_ref(ref_type));
}

extern "C"
void
fn_ref_destroy(FN_ref_t *p)
{
	delete (FN_ref *)p;
}

extern "C"
FN_ref_t *
fn_ref_copy(const FN_ref_t *p)
{
	return ((FN_ref_t *)
		new FN_ref(*((const FN_ref *)p)));
}

extern "C"
FN_ref_t *
fn_ref_assign(FN_ref_t *p1, const FN_ref_t *p2)
{
	return ((FN_ref_t *)
		&(*((FN_ref *)p1) = *((const FN_ref *)p2)));
}


extern "C"
const FN_identifier_t *
fn_ref_type(const FN_ref_t *p)
{
	const FN_identifier *ident = ((const FN_ref *)p)->type();
	if (ident)
		return (&(ident->info));
	else
		return (0);
}

extern "C"
unsigned
fn_ref_addrcount(const FN_ref_t *p)
{
	return (((const FN_ref *)p)->addrcount());
}

extern "C"
const FN_ref_addr_t *
fn_ref_first(const FN_ref_t *p, void **iter_pos)
{
	return ((const FN_ref_addr_t *)
		(((const FN_ref *)p)->first(*iter_pos)));
}

extern "C"
const FN_ref_addr_t *
fn_ref_next(const FN_ref_t *p, void **iter_pos)
{
	return ((const FN_ref_addr_t *)
		(((const FN_ref *)p)->next(*iter_pos)));
}

extern "C"
FN_string_t *
fn_ref_description(const FN_ref_t *p, unsigned detail, unsigned *more_detail)
{
	FN_string *desc = ((const FN_ref *)p)->description(detail, more_detail);
	return ((FN_string_t *)desc);
}

extern "C"
int
fn_ref_prepend_addr(FN_ref_t *p, const FN_ref_addr_t *a)
{
	return (((FN_ref *)p)->prepend_addr(
					    *((const FN_ref_addr *)a)));
}

extern "C"
int
fn_ref_append_addr(FN_ref_t *p, const FN_ref_addr_t *a)
{
	return (((FN_ref *)p)->append_addr(*((const FN_ref_addr *)a)));
}

extern "C"
int
fn_ref_insert_addr(FN_ref_t *p,
    void **iter_pos, const FN_ref_addr_t *a)
{
	return (((FN_ref *)p)->insert_addr(*iter_pos,
	    *((const FN_ref_addr *)a)));
}

extern "C"
int
fn_ref_delete_addr(FN_ref_t *p, void **iter_pos)
{
	return (((FN_ref *)p)->delete_addr(*iter_pos));
}

extern "C"
int
fn_ref_delete_all(FN_ref_t *p)
{
	return (((FN_ref *)p)->delete_all());
}

extern "C"
FN_ref_t *
fn_ref_create_link(const FN_composite_name_t *link_name)
{
	return ((FN_ref_t *)FN_ref::create_link(
	    *((const FN_composite_name*)link_name)));
}

extern "C"
int
fn_ref_is_link(const FN_ref_t *p)
{
	return (((FN_ref *)p)->is_link());
}

extern "C"
FN_composite_name_t *
fn_ref_link_name(const FN_ref_t *link_ref)
{
	return ((FN_composite_name_t *) (((FN_ref *)link_ref)->link_name()));
}
