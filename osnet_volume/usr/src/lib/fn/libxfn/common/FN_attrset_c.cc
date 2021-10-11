/*
 * Copyright (c) 1992 - 1993 by Sun Microsystems, Inc.
 */

#pragma ident "@(#)FN_attrset_c.cc	1.2 94/07/30 SMI"

#include <xfn/FN_attrset.hh>


extern "C"
FN_attrset_t *
fn_attrset_create()
{
	return ((FN_attrset_t *)new FN_attrset());
}

extern "C"
void
fn_attrset_destroy(FN_attrset_t *p)
{
	delete (FN_attrset *)p;
}

extern "C"
FN_attrset_t *
fn_attrset_copy(const FN_attrset_t *p)
{
	return ((FN_attrset_t *)
		new FN_attrset(*((const FN_attrset *)p)));
}

extern "C"
FN_attrset_t *
fn_attrset_assign(FN_attrset_t *p1,
		const FN_attrset_t *p2)
{
	return ((FN_attrset_t *)
		&(*((FN_attrset *)p1) =
		*((const FN_attrset *)p2)));
}

extern "C"
const FN_attribute_t *
fn_attrset_get(const FN_attrset_t *p,
		const FN_identifier_t *attr)
{
	FN_identifier attr_id(*attr);
	return ((const FN_attribute_t *)((const FN_attrset *)p)->get(attr_id));
}

extern "C"
unsigned
fn_attrset_count(const FN_attrset_t *p)
{
	return (((const FN_attrset *)p)->count());
}

extern "C"
const FN_attribute_t *
fn_attrset_first(const FN_attrset_t *p, void **iter_pos)
{
	return ((const FN_attribute_t *)
		(((const FN_attrset *)p)->first(*iter_pos)));
}

extern "C"
const FN_attribute_t *
fn_attrset_next(const FN_attrset_t *p, void **iter_pos)
{
	return ((const FN_attribute_t *)
		(((const FN_attrset *)p)->next(*iter_pos)));
}

extern "C"
int
fn_attrset_add(FN_attrset_t *p, const FN_attribute_t *attr,
    unsigned int exclusive)
{
	return (((FN_attrset *)p)->add(*(const FN_attribute *)attr,
	    exclusive));
}

extern "C"
int
fn_attrset_remove(FN_attrset_t *p, const FN_identifier_t *attr)
{
	FN_identifier attr_id(*attr);
	return (((FN_attrset *)p)->remove(attr_id));
}
