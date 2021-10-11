/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_composite_name_c.cc	1.3	96/03/31 SMI"

#include <xfn/FN_composite_name.hh>


extern "C"
FN_composite_name_t *
fn_composite_name_create()
{
	return ((FN_composite_name_t *)new FN_composite_name);
}

extern "C"
void
fn_composite_name_destroy(FN_composite_name_t *p)
{
	delete (FN_composite_name *)p;
}

extern "C"
FN_string_t *
fn_string_from_composite_name(const FN_composite_name_t *p,
    unsigned int *status)
{
	// optional status checked by string()
	FN_string_t *answer = ((FN_string_t *)
	    ((const FN_composite_name *)p)->string(status));
	return (answer);
}

extern "C"
FN_composite_name_t *
fn_composite_name_from_string(const FN_string_t *s)
{
	return ((FN_composite_name_t *)new
	    FN_composite_name(*((const FN_string *)s)));
}

extern "C"
FN_composite_name_t *
fn_composite_name_assign_string(FN_composite_name *p, const FN_string *s)
{
	return ((FN_composite_name_t *)
		&(*((FN_composite_name *)p) = *((const FN_string *)s)));
}

extern "C"
FN_composite_name_t *
fn_composite_name_from_str(const unsigned char *s)
{
	return ((FN_composite_name_t *)new FN_composite_name(s));
}

extern "C"
FN_composite_name_t *
fn_composite_name_copy(const FN_composite_name_t *p)
{
	return ((FN_composite_name_t *)new FN_composite_name(
	    *((const FN_composite_name *)p)));
}

extern "C"
FN_composite_name_t *
fn_composite_name_assign(FN_composite_name_t *p1,
    const FN_composite_name_t *p2)
{
	return ((FN_composite_name_t *)&(*((FN_composite_name *)p1) =
					 *((const FN_composite_name *)p2)));
}

extern "C"
unsigned int
fn_composite_name_count(const FN_composite_name_t *p)
{
	return (((const FN_composite_name *)p)->count());
}

extern "C"
int
fn_composite_name_is_empty(const FN_composite_name_t *p)
{
	return (((const FN_composite_name *)p)->is_empty());
}

extern "C"
int
fn_composite_name_is_equal(
	const FN_composite_name_t *p1,
	const FN_composite_name_t *p2,
	unsigned int *status)
{
	// optional status checking done by is_equal()
	int answer =
	    ((const FN_composite_name *)p1)->
		is_equal(*((const FN_composite_name *)p2), status);
	return (answer);
}

extern "C"
const FN_string_t *
fn_composite_name_first(const FN_composite_name_t *p, void **iter_pos)
{
	return ((const FN_string_t *)
		(((const FN_composite_name *)p)->first(*iter_pos)));
}

extern "C"
int
fn_composite_name_is_prefix(
			    const FN_composite_name_t *p1,
			    const FN_composite_name_t *p2,
			    void **iter_pos,
			    unsigned int *status)
{
	// optional status argument done by is_prefix()
	int answer =
	    ((const FN_composite_name *)p1)->
		is_prefix(*((const FN_composite_name *)p2), *iter_pos, status);
	return (answer);
}

extern "C"
const FN_string_t *
fn_composite_name_last(const FN_composite_name_t *p, void **iter_pos)
{
	return ((const FN_string_t *)
		((const FN_composite_name *)p)->last(*iter_pos));
}

extern "C"
int
fn_composite_name_is_suffix(
			    const FN_composite_name_t *p1,
			    const FN_composite_name_t *p2,
			    void **iter_pos,
			    unsigned int *status)
{
	// optional status argument done by is_suffix()
	int answer =
	    ((const FN_composite_name *)p1)->
		is_suffix(*((const FN_composite_name *)p2), *iter_pos, status);
	return (answer);
}

extern "C"
const FN_string_t *
fn_composite_name_next(const FN_composite_name_t *p, void **iter_pos)
{
	return ((const FN_string_t *)
		((const FN_composite_name *)p)->next(*iter_pos));
}

extern "C"
const FN_string_t *
fn_composite_name_prev(const FN_composite_name_t *p, void **iter_pos)
{
	return ((const FN_string_t *)
		((const FN_composite_name *)p)->prev(*iter_pos));
}

extern "C"
FN_composite_name_t *
fn_composite_name_prefix(const FN_composite_name_t *p, const void *iter_pos)
{
	return ((FN_composite_name_t *)
		((const FN_composite_name *)p)->prefix(iter_pos));
}

extern "C"
FN_composite_name_t *
fn_composite_name_suffix(const FN_composite_name_t *p, const void *iter_pos)
{
	return ((FN_composite_name_t *)
		((const FN_composite_name *)p)->suffix(iter_pos));
}

extern "C"
int
fn_composite_name_prepend_comp(FN_composite_name_t *p, const FN_string_t *c)
{
	return (((FN_composite_name *)p)->prepend_comp(
	    *((const FN_string *)c)));
}

extern "C"
int
fn_composite_name_prepend_name(FN_composite_name_t *p,
    const FN_composite_name_t *n)
{
	return (((FN_composite_name *)p)->prepend_name(
	    *((const FN_composite_name *)n)));
}

extern "C"
int
fn_composite_name_append_comp(FN_composite_name_t *p, const FN_string_t *c)
{
	return (((FN_composite_name *)p)->append_comp(*((const FN_string *)c)));
}

extern "C"
int
fn_composite_name_append_name(FN_composite_name_t *p,
    const FN_composite_name_t *n)
{
	return (((FN_composite_name *)p)->append_name(
	    *((const FN_composite_name *)n)));
}

extern "C"
int
fn_composite_name_insert_comp(FN_composite_name_t *p,
    void **iter_pos, const FN_string_t *c)
{
	return (((FN_composite_name *)p)->insert_comp(*iter_pos,
	    *((const FN_string *)c)));
}

extern "C"
int
fn_composite_name_insert_name(FN_composite_name_t *p,
    void **iter_pos, const FN_composite_name_t *n)
{
	return (((FN_composite_name *)p)->insert_name(*iter_pos,
	    *((const FN_composite_name *)n)));
}

extern "C"
int
fn_composite_name_delete_comp(FN_composite_name_t *p, void **iter_pos)
{
	return (((FN_composite_name *)p)->delete_comp(*iter_pos));
}

#if 0
extern "C"
int
fn_composite_name_delete_all(FN_composite_name_t *p)
{
	return (((FN_composite_name *)p)->delete_all());
}
#endif
