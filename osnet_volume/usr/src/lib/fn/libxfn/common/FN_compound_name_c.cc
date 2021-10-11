/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */


#pragma ident	"@(#)FN_compound_name_c.cc	1.4	96/03/31 SMI"

#include <xfn/FN_compound_name.hh>

extern "C"
void
fn_compound_name_destroy(FN_compound_name_t *p)
{
	delete (FN_compound_name *)p;
}

extern "C"
FN_compound_name_t *
fn_compound_name_from_syntax_attrs(
	const FN_attrset_t *p,
	const FN_string_t *name,
	FN_status_t *s)
{
	if (s)
		return ((FN_compound_name_t *)
			FN_compound_name::from_syntax_attrs(*((FN_attrset *)p),
			    *((FN_string *)name), *((FN_status *)s)));
	else {
		FN_status sub_s;
		return ((FN_compound_name_t *)
			FN_compound_name::from_syntax_attrs(*((FN_attrset *)p),
			    *((FN_string *)name), sub_s));
	}
}

extern "C"
FN_attrset_t *
fn_compound_name_get_syntax_attrs(const FN_compound_name_t *p)
{
	return ((FN_attrset_t *)
		((const FN_compound_name *)p)->get_syntax_attrs());
}

extern "C"
FN_string_t *
fn_string_from_compound_name(const FN_compound_name_t *p)
{
	return ((FN_string_t *)((const FN_compound_name *)p)->string());
}

extern "C"
FN_compound_name_t *
fn_compound_name_copy(const FN_compound_name_t *p)
{
	const FN_compound_name	*n;

	n = (const FN_compound_name *)p;
	return ((FN_compound_name_t *)n->dup());
}

extern "C"
FN_compound_name_t *
fn_compound_name_assign(FN_compound_name_t *dst,
    const FN_compound_name_t *src)
{
	return ((FN_compound_name_t *)
	    &(*((FN_compound_name *)dst) = *((const FN_compound_name *)src)));
}

extern "C"
unsigned
fn_compound_name_count(const FN_compound_name_t *p)
{
	return (((const FN_compound_name *)p)->count());
}

extern "C"
int
fn_compound_name_is_empty(const FN_compound_name_t *p)
{
	return (((const FN_compound_name *)p)->is_empty());
}

extern "C"
int
fn_compound_name_is_equal(
	const FN_compound_name_t *p1,
	const FN_compound_name_t *p2,
	unsigned int *status)
{
	const FN_compound_name	*n1, *n2;

	n1 = (const FN_compound_name *)p1;
	n2 = (const FN_compound_name *)p2;
	// optional status is handled by is_equal()
	return (n1->is_equal(*n2, status));
}

extern "C"
const FN_string_t *
fn_compound_name_first(const FN_compound_name_t *p, void **iter_pos)
{
	return ((const FN_string_t *)
		(((const FN_compound_name *)p)->first(*iter_pos)));
}

extern "C"
int
fn_compound_name_is_prefix(
	const FN_compound_name_t *p1,
	const FN_compound_name_t *p2,
	void **iter_pos,
	unsigned int *status)
{
	// optional status is handled by is_prefix()
	int answer = ((const FN_compound_name *)p1)->
	    is_prefix(*((const FN_compound_name *)p2), *iter_pos, status);
	return (answer);
}

extern "C"
const FN_string_t *
fn_compound_name_last(const FN_compound_name_t *p, void **iter_pos)
{
	return ((const FN_string_t *)
		((const FN_compound_name *)p)->last(*iter_pos));
}

extern "C"
int
fn_compound_name_is_suffix(
	const FN_compound_name_t *p1,
	const FN_compound_name_t *p2,
	void **iter_pos,
	unsigned int *status)
{
	// optional status is handled by is_suffix()
	int answer = ((const FN_compound_name *)p1)->
	    is_suffix(*((const FN_compound_name *)p2), *iter_pos,
		status);
	return (answer);
}

extern "C"
const FN_string_t *
fn_compound_name_next(const FN_compound_name_t *p, void **iter_pos)
{
	return ((const FN_string_t *)
		((const FN_compound_name *)p)->next(*iter_pos));
}

extern "C"
const FN_string_t *
fn_compound_name_prev(const FN_compound_name_t *p, void **iter_pos)
{
	return ((const FN_string_t *)
		((const FN_compound_name *)p)->prev(*iter_pos));
}

extern "C"
FN_compound_name_t *
fn_compound_name_prefix(const FN_compound_name_t *p, const void *iter_pos)
{
	return ((FN_compound_name_t *)
		((const FN_compound_name *)p)->prefix(iter_pos));
}

extern "C"
FN_compound_name_t *
fn_compound_name_suffix(const FN_compound_name_t *p, const void *iter_pos)
{
	return ((FN_compound_name_t *)
		((const FN_compound_name *)p)->suffix(iter_pos));
}

extern "C"
int
fn_compound_name_prepend_comp(
	FN_compound_name_t *p,
	const FN_string_t *c,
	unsigned int *status)
{
	// optional status is handled by prepend_comp()
	int answer = ((FN_compound_name *)p)->prepend_comp(
	    *((const FN_string *)c), status);
	return (answer);
}

extern "C"
int
fn_compound_name_append_comp(
	FN_compound_name_t *p,
	const FN_string_t *c,
	unsigned int *status)
{
	// optional status is handled by append_comp()
	int answer = ((FN_compound_name *)p)->append_comp(
	    *((const FN_string *)c), status);
	return (answer);
}

extern "C"
int
fn_compound_name_insert_comp(
	FN_compound_name_t *p,
	void **iter_pos,
	const FN_string_t *c,
	unsigned int *status)
{
	// optional status is handled by insert_comp()
	int answer = ((FN_compound_name *)p)->
	    insert_comp(*iter_pos, *((const FN_string *)c), status);
	return (answer);
}

extern "C"
int
fn_compound_name_delete_comp(FN_compound_name_t *p, void **iter_pos)
{
	return (((FN_compound_name *)p)->delete_comp(*iter_pos));
}

extern "C"
int
fn_compound_name_delete_all(FN_compound_name_t *p)
{
	return (((FN_compound_name *)p)->delete_all());
}
