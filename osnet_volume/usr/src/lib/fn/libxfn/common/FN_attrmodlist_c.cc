/*
 * Copyright (c) 1992 - 1993 by Sun Microsystems, Inc.
 */

#pragma ident "@(#)FN_attrmodlist_c.cc	1.2 94/07/30 SMI"

#include <xfn/FN_attrmodlist.hh>


extern "C"
FN_attrmodlist_t *
fn_attrmodlist_create()
{
	return ((FN_attrmodlist_t *)new FN_attrmodlist());
}

extern "C"
void
fn_attrmodlist_destroy(FN_attrmodlist_t *p)
{
	delete (FN_attrmodlist *)p;
}

extern "C"
FN_attrmodlist_t *
fn_attrmodlist_copy(const FN_attrmodlist_t *p)
{
	return ((FN_attrmodlist_t *)
		new FN_attrmodlist(*((const FN_attrmodlist *)p)));
}

extern "C"
FN_attrmodlist_t *
fn_attrmodlist_assign(FN_attrmodlist_t *p1,
		const FN_attrmodlist_t *p2)
{
	return ((FN_attrmodlist_t *)
		&(*((FN_attrmodlist *)p1) =
		*((const FN_attrmodlist *)p2)));
}

extern "C"
unsigned
fn_attrmodlist_count(const FN_attrmodlist_t *p)
{
	return (((const FN_attrmodlist *)p)->count());
}

extern "C"
const FN_attribute_t *
fn_attrmodlist_first(const FN_attrmodlist_t *p, void **iter_pos,
    unsigned int *first_mod_op)
{
	return ((const FN_attribute_t *)
		(((const FN_attrmodlist *)p)->first(*iter_pos,
						    *first_mod_op)));
}

extern "C"
const FN_attribute_t *
fn_attrmodlist_next(const FN_attrmodlist_t *p, void **iter_pos,
		    unsigned int *mod_op)
{
	return ((const FN_attribute_t *)
	    (((const FN_attrmodlist *)p)->next(*iter_pos, *mod_op)));
}

extern "C"
int
fn_attrmodlist_add(FN_attrmodlist_t *p, unsigned int mod_op,
    const FN_attribute_t *mod_args)
{
	return (((FN_attrmodlist *)p)->add(mod_op,
	    *(const FN_attribute *)mod_args));
}
