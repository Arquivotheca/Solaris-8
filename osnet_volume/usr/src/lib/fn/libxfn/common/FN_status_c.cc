/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)FN_status_c.cc	1.5	96/03/31 SMI"

#include <xfn/FN_status.hh>

extern "C"
FN_status_t *
fn_status_create()
{
	return ((FN_status_t *)new FN_status());
}

extern "C"
void
fn_status_destroy(FN_status_t *p)
{
	delete (FN_status *)p;
}

extern "C"
FN_status_t *
fn_status_copy(const FN_status_t *p)
{
	return ((FN_status_t *)
		new FN_status(*((const FN_status *)p)));
}

extern "C"
FN_status_t *
fn_status_assign(FN_status_t *p1, const FN_status_t *p2)
{
	return ((FN_status_t *)
		&(*((FN_status *)p1) =
		*((const FN_status *)p2)));
}

extern "C"
unsigned int
fn_status_code(const FN_status_t *p)
{
	return (((const FN_status *)p)->code());
}

extern "C"
const FN_ref_t *
fn_status_resolved_ref(const FN_status_t *p)
{
	return ((const FN_ref_t *)
		(((const FN_status *)p)->resolved_ref()));
}

extern "C"
const FN_composite_name_t *
fn_status_remaining_name(const FN_status_t *p)
{
	return ((const FN_composite_name_t *)
		(((const FN_status *)p)->remaining_name()));
}

extern "C"
const FN_composite_name_t *
fn_status_resolved_name(const FN_status_t *p)
{
	return ((const FN_composite_name_t *)
		(((const FN_status *)p)->resolved_name()));
}

extern "C"
unsigned int
fn_status_link_code(const FN_status_t *p)
{
	return (((const FN_status *)p)->link_code());
}

extern "C"
const FN_ref_t *
fn_status_link_resolved_ref(const FN_status_t *p)
{
	return ((const FN_ref_t *)
		(((const FN_status *)p)->link_resolved_ref()));
}

extern "C"
const FN_composite_name_t *
fn_status_link_remaining_name(const FN_status_t *p)
{
	return ((const FN_composite_name_t *)
		(((const FN_status *)p)->link_remaining_name()));
}

extern "C"
const FN_composite_name_t *
fn_status_link_resolved_name(const FN_status_t *p)
{
	return ((const FN_composite_name_t *)
		(((const FN_status *)p)->resolved_name()));
}

extern "C"
int
fn_status_is_success(const FN_status_t *p)
{
	return (((const FN_status *)p)->is_success());
}

extern "C"
FN_string_t *
fn_status_description(
	const FN_status_t *p,
	unsigned detail,
	unsigned *more_detail)
{
	return ((FN_string_t *)
		((const FN_status *)p)->description(detail, more_detail));
}


extern "C"
int
fn_status_set(
	FN_status_t *p,
	unsigned int code,
	const FN_ref_t *resolved_reference,
	const FN_composite_name_t *resolved_name,
	const FN_composite_name_t *remaining_name)
{
	return (((FN_status *)p)->set(code,
	    (const FN_ref *)resolved_reference,
	    (const FN_composite_name *)resolved_name,
	    (const FN_composite_name *)remaining_name));
}

extern "C"
int
fn_status_set_success(FN_status_t *p)
{
	return (((FN_status *)p)->set_success());
}

extern "C"
int
fn_status_set_code(FN_status_t *p, unsigned int code)
{
	return (((FN_status *)p)->set_code(code));
}

extern "C"
int
fn_status_set_resolved_ref(FN_status_t *p, const FN_ref_t *ref)
{
	return (((FN_status *)p)->set_resolved_ref((FN_ref *)ref));
}

extern "C"
int
fn_status_set_remaining_name(FN_status_t *p, const FN_composite_name_t *name)
{
	return (((FN_status *)p)->set_remaining_name(
	    (FN_composite_name *)name));
}

extern "C"
int
fn_status_set_resolved_name(FN_status_t *p, const FN_composite_name_t *name)
{
	return (((FN_status *)p)->set_resolved_name(
		(FN_composite_name *)name));
}

extern "C"
int
fn_status_append_remaining_name(FN_status_t *p, const FN_composite_name_t *name)
{
	return (((FN_status *)p)->append_remaining_name(
		*((FN_composite_name *)name)));
}

extern "C"
int
fn_status_append_resolved_name(FN_status_t *p, const FN_composite_name_t *name)
{
	return (((FN_status *)p)->append_resolved_name(
		*((FN_composite_name *)name)));
}

extern "C" int
fn_status_advance_by_name(
	FN_status_t *p,
	const FN_composite_name_t *prefix,
	const FN_ref_t *resolved_ref)
{
	return ((FN_status *)p)->advance_by_name(
	    *((FN_composite_name *)prefix),
	    *((FN_ref *)resolved_ref));
}


extern "C"
int
fn_status_set_link_code(FN_status_t *p, unsigned int code)
{
	return (((FN_status *)p)->set_link_code(code));
}

extern "C"
int
fn_status_set_link_resolved_ref(FN_status_t *p, const FN_ref_t *ref)
{
	return (((FN_status *)p)->set_link_resolved_ref((FN_ref *)ref));
}

extern "C"
int
fn_status_set_link_remaining_name(FN_status_t *p,
    const FN_composite_name_t *name)
{
	return (((FN_status *)p)->set_link_remaining_name(
		(FN_composite_name *)name));
}

extern "C"
int
fn_status_set_link_resolved_name(FN_status_t *p,
    const FN_composite_name_t *name)
{
	return (((FN_status *)p)->set_link_resolved_name(
		(FN_composite_name *)name));
}

extern "C"
const FN_string_t *
fn_status_diagnostic_message(const FN_status_t *stat)
{
	return ((const FN_string_t *)
		((FN_status *)stat)->diagnostic_message());
}

extern "C"
const FN_string_t *
fn_status_link_diagnostic_message(const FN_status_t *stat)
{
	return ((const FN_string_t *)
		((FN_status *)stat)->link_diagnostic_message());
}

extern "C"
int
fn_status_set_diagnostic_message(FN_status_t *stat,
    const FN_string_t *msg)
{
	return (((FN_status *)stat)->set_diagnostic_message((FN_string *)msg));
}

extern "C"
int
fn_status_set_link_diagnostic_message(FN_status_t *stat,
    const FN_string_t *msg)
{
	return (((FN_status *)stat)->set_link_diagnostic_message(
	    (FN_string *)msg));
}
