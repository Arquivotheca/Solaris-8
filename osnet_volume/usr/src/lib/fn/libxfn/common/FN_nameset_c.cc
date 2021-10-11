/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#pragma ident "@(#)FN_nameset_c.cc	1.2 94/07/31 SMI"

#include <xfn/FN_nameset.hh>


extern "C"
FN_nameset_t *
fn_nameset_create()
{
	return ((FN_nameset_t *)new FN_nameset());
}

extern "C"
void
fn_nameset_destroy(FN_nameset_t *p)
{
	delete (FN_nameset *)p;
}

extern "C"
FN_nameset_t *
fn_nameset_copy(const FN_nameset_t *p)
{
	return ((FN_nameset_t *)
		new FN_nameset(*((const FN_nameset *)p)));
}

extern "C"
FN_nameset_t *
fn_nameset_assign(FN_nameset_t *p1,
		const FN_nameset_t *p2)
{
	return ((FN_nameset_t *)
		&(*((FN_nameset *)p1) =
		*((const FN_nameset *)p2)));
}

extern "C"
unsigned
fn_nameset_count(const FN_nameset_t *p)
{
	return (((const FN_nameset *)p)->count());
}

extern "C"
const FN_string_t *
fn_nameset_first(const FN_nameset_t *p, void **iter_pos)
{
	return ((const FN_string_t *)
		(((const FN_nameset *)p)->first(*iter_pos)));
}

extern "C"
const FN_string_t *
fn_nameset_next(const FN_nameset_t *p, void **iter_pos)
{
	return ((const FN_string_t *)
		(((const FN_nameset *)p)->next(*iter_pos)));
}

extern "C"
int
fn_nameset_add(FN_nameset_t *p, const FN_string_t *name,
		unsigned int exclusive)
{
	return (((FN_nameset *)p)->add(*((const FN_string *)name), exclusive));
}

extern "C"
int
fn_nameset_remove(FN_nameset_t *p, const FN_string_t *name)
{
	return (((FN_nameset *)p)->remove(
		*((const FN_string *)name)));
}
