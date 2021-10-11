/*
 * Copyright (c) 1993 - 1994 by Sun Microsystems, Inc.
 */

#pragma ident "@(#)FN_bindingset_c.cc	1.2 94/07/30 SMI"

#include <xfn/FN_bindingset.hh>


extern "C"
FN_bindingset_t *
fn_bindingset_create()
{
	return ((FN_bindingset_t *)new FN_bindingset());
}

extern "C"
void
fn_bindingset_destroy(FN_bindingset_t *p)
{
	delete (FN_bindingset *)p;
}

extern "C"
FN_bindingset_t *
fn_bindingset_copy(const FN_bindingset_t *p)
{
	return ((FN_bindingset_t *)new FN_bindingset(
	    *((const FN_bindingset *)p)));
}

extern "C"
FN_bindingset_t *
fn_bindingset_assign(FN_bindingset_t *p1,
		const FN_bindingset_t *p2)
{
	return ((FN_bindingset_t *)
	    &(*((FN_bindingset *)p1) =
	    *((const FN_bindingset *)p2)));
}

extern "C"
const FN_ref_t *
fn_bindingset_get_ref(const FN_bindingset_t *p,
		const FN_string_t *name)
{
	return ((const FN_ref_t *)
	    (((const FN_bindingset *)p)->get_ref(
	    *((const FN_string *)name))));
}

extern "C"
unsigned
fn_bindingset_count(const FN_bindingset_t *p)
{
	return (((const FN_bindingset *)p)->count());
}

extern "C"
const FN_string_t *
fn_bindingset_first(const FN_bindingset_t *p,
    void **iter_pos, const FN_ref_t **ref)
{
	return ((const FN_string_t *)
	    (((const FN_bindingset *)p)->first(*iter_pos,
	    *((const FN_ref **)ref))));
}

extern "C"
const FN_string_t *
fn_bindingset_next(const FN_bindingset_t *p,
    void **iter_pos, const FN_ref_t **ref)
{
	return ((const FN_string_t *)
	    (((const FN_bindingset *)p)->next(*iter_pos,
	    *((const FN_ref **)ref))));
}

extern "C"
int
fn_bindingset_add(FN_bindingset_t *p, const FN_string_t *name,
    const FN_ref_t *ref, unsigned int exclusive)
{
	return (((FN_bindingset *)p)->add(*((const FN_string *)name),
	    *((const FN_ref *)ref), exclusive));
}

extern "C"
int
fn_bindingset_remove(FN_bindingset_t *p, const FN_string_t *name)
{
	return (((FN_bindingset *)p)->remove(*((const FN_string *)name)));
}
