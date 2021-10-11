/*
 * Copyright (c) 1993-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_attribute_c.cc	1.3	96/03/31 SMI"

#include <xfn/FN_attrvalue.hh>
#include <xfn/FN_identifier.hh>
#include <xfn/FN_attribute.hh>

FN_attribute_t *
fn_attribute_create(const FN_identifier_t *attr_id,
    const FN_identifier_t *attr_syntax)
{
	FN_identifier a_id(*attr_id);
	FN_identifier a_syn(*attr_syntax);
	return ((FN_attribute_t *) new FN_attribute(a_id, a_syn));
}

void
fn_attribute_destroy(FN_attribute_t *ptr)
{
	delete (FN_attribute *)ptr;
}

FN_attribute_t *
fn_attribute_copy(const FN_attribute_t *orig)
{
	return ((FN_attribute_t *)
		new FN_attribute(*(const FN_attribute *)orig));
}

FN_attribute_t *
fn_attribute_assign(FN_attribute_t *dst, const FN_attribute_t *src)
{
	return ((FN_attribute_t *)
		&(*((FN_attribute *)dst) = *(const FN_attribute *)src));
}

const FN_identifier_t *
fn_attribute_identifier(const FN_attribute_t *attr)
{
	const FN_identifier *ident = ((const FN_attribute *)attr)->identifier();
	if (ident)
	    return (&(ident->info));
	else
	    return (0);
}

const FN_identifier_t *
fn_attribute_syntax(const FN_attribute_t *attr)
{
	const FN_identifier *ident = ((const FN_attribute *)attr)->syntax();
	if (ident)
	    return (&(ident->info));
	else
	    return (0);
}

unsigned int
fn_attribute_valuecount(const FN_attribute_t *attr)
{
	return (((const FN_attribute *)attr)->valuecount());
}

const FN_attrvalue_t *
fn_attribute_first(const FN_attribute_t *attr, void **iter_pos)
{
	const FN_attrvalue *val =
	    ((const FN_attribute *)attr)->first(*iter_pos);

	if (val)
	    return (&(val->value));
	else
	    return (0);
}

const FN_attrvalue_t *
fn_attribute_next(const FN_attribute_t *attr, void **iter_pos)
{
	const FN_attrvalue *val = ((const FN_attribute *)attr)->next(*iter_pos);

	if (val)
	    return (&(val->value));
	else
	    return (0);
}

int
fn_attribute_add(FN_attribute_t *attr, const FN_attrvalue_t *val_t,
    unsigned int exclusive)
{
	FN_attrvalue val(*val_t);
	return (((FN_attribute *)attr)->add(val, exclusive));
}

int
fn_attribute_remove(FN_attribute_t *attr, const FN_attrvalue_t *val_t)
{
	FN_attrvalue val(*val_t);
	return (((FN_attribute *)attr)->remove(val));
}
