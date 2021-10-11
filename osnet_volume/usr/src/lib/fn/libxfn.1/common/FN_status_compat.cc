/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
*/
#pragma ident	"@(#)FN_status_compat.cc	1.1	96/03/31 SMI"

#include "fake_xfn.hh"

typedef FN_status_t *(*fn_status_create_func)(void);

extern "C" FN_status_t *
fn_status_create(void)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_create", &func);

	return (((fn_status_create_func)func)());
}

typedef void (*fn_status_destroy_func)(FN_status_t *);

extern "C" void
fn_status_destroy(FN_status_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_destroy", &func);

	((fn_status_destroy_func)func)(arg0);
}

typedef FN_status_t *(*fn_status_copy_func)(const FN_status_t *);

extern "C" FN_status_t *
fn_status_copy(const FN_status_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_copy", &func);

	return (((fn_status_copy_func)func)(arg0));
}

typedef FN_status_t *(*fn_status_assign_func)(
	FN_status_t *dst,
	const FN_status_t *src);

extern "C" FN_status_t *
fn_status_assign(FN_status_t *dst, const FN_status_t *src)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_assign", &func);

	return (((fn_status_assign_func)func)(dst, src));
}

typedef unsigned int (*fn_status_code_func)(const FN_status_t *);

extern "C" unsigned int
fn_status_code(const FN_status_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_code", &func);

	return (((fn_status_code_func)func)(arg0));
}

typedef const FN_composite_name_t *(*fn_status_remaining_name_func)(
	const FN_status_t *);

extern "C" const FN_composite_name_t *
fn_status_remaining_name(const FN_status_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_remaining_name", &func);

	return (((fn_status_remaining_name_func)func)(arg0));
}

typedef const FN_composite_name_t *(*fn_status_resolved_name_func)(
	const FN_status_t *);

extern "C" const FN_composite_name_t *
fn_status_resolved_name(const FN_status_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_resolved_name", &func);

	return (((fn_status_resolved_name_func)func)(arg0));
}

typedef const FN_ref_t *(*fn_status_resolved_ref_func)(const FN_status_t *);

extern "C" const FN_ref_t *
fn_status_resolved_ref(const FN_status_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_resolved_ref", &func);

	return (((fn_status_resolved_ref_func)func)(arg0));
}

typedef const FN_string_t *(*fn_status_diagnostic_message_func)(
	const FN_status_t *stat);

extern "C" const FN_string_t *
fn_status_diagnostic_message(const FN_status_t *stat)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_diagnostic_message", &func);

	return (((fn_status_diagnostic_message_func)func)(stat));
}

typedef unsigned int (*fn_status_link_code_func)(const FN_status_t *);

extern "C" unsigned int
fn_status_link_code(const FN_status_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_link_code", &func);

	return (((fn_status_link_code_func)func)(arg0));
}

typedef const FN_composite_name_t *(*fn_status_link_remaining_name_func)(
	const FN_status_t *);

extern "C" const FN_composite_name_t *
fn_status_link_remaining_name(
		const FN_status_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_link_remaining_name", &func);

	return (((fn_status_link_remaining_name_func)func)(arg0));
}

typedef const FN_composite_name_t *(*fn_status_link_resolved_name_func)(
	const FN_status_t *);

extern "C" const FN_composite_name_t *
fn_status_link_resolved_name(const FN_status_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_link_resolved_name", &func);

	return (((fn_status_link_resolved_name_func)func)(arg0));
}

typedef const FN_ref_t *(*fn_status_link_resolved_ref_func)(
	const FN_status_t *);

extern "C" const FN_ref_t *
fn_status_link_resolved_ref(const FN_status_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_link_resolved_ref", &func);

	return (((fn_status_link_resolved_ref_func)func)(arg0));
}

typedef const FN_string_t *(*fn_status_link_diagnostic_message_func)(
	const FN_status_t *stat);

extern "C" const FN_string_t *
fn_status_link_diagnostic_message(const FN_status_t *stat)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_link_diagnostic_message", &func);

	return (((fn_status_link_diagnostic_message_func)func)(stat));
}

typedef int (*fn_status_is_success_func)(const FN_status_t *);

extern "C" int
fn_status_is_success(const FN_status_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_is_success", &func);

	return (((fn_status_is_success_func)func)(arg0));
}

typedef int (*fn_status_set_success_func)(FN_status_t *);

extern "C" int
fn_status_set_success(FN_status_t *arg0)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_set_success", &func);

	return (((fn_status_set_success_func)func)(arg0));
}

typedef int (*fn_status_set_func)(
	FN_status_t *,
	unsigned int code,
	const FN_ref_t *resolved_ref,
	const FN_composite_name_t *resolved_name,
	const FN_composite_name_t *remaining_name);

extern "C" int
fn_status_set(FN_status_t *arg0, unsigned int code,
	const FN_ref_t *resolved_ref,
	const FN_composite_name_t *resolved_name,
	const FN_composite_name_t *remaining_name)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_set", &func);

	return (((fn_status_set_func)func)(arg0, code, resolved_ref,
	    resolved_name, remaining_name));
}

typedef int (*fn_status_set_code_func)(FN_status_t *, unsigned int code);

extern "C" int
fn_status_set_code(FN_status_t *arg0, unsigned int code)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_set_code", &func);

	return (((fn_status_set_code_func)func)(arg0, code));
}

typedef int (*fn_status_set_remaining_name_func)(
	FN_status_t *,
	const FN_composite_name_t *);

extern "C" int
fn_status_set_remaining_name(FN_status_t *arg0,
	const FN_composite_name_t *arg1)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_set_remaining_name", &func);

	return (((fn_status_set_remaining_name_func)func)(arg0, arg1));
}

typedef int (*fn_status_set_resolved_name_func)(
	FN_status_t *,
	const FN_composite_name_t *);

extern "C" int
fn_status_set_resolved_name(FN_status_t *arg0,
	const FN_composite_name_t *arg1)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_set_resolved_name", &func);

	return (((fn_status_set_resolved_name_func)func)(arg0, arg1));
}

typedef int (*fn_status_set_resolved_ref_func)(FN_status_t *,
	const FN_ref_t *);

extern "C" int
fn_status_set_resolved_ref(FN_status_t *arg0,  const FN_ref_t *arg1)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_set_resolved_ref", &func);

	return (((fn_status_set_resolved_ref_func)func)(arg0, arg1));
}

typedef int (*fn_status_set_diagnostic_message_func)(
	FN_status_t *stat,
	const FN_string_t *msg);

extern "C" int
fn_status_set_diagnostic_message(FN_status_t *stat, const FN_string_t *msg)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_set_diagnostic_message", &func);

	return (((fn_status_set_diagnostic_message_func)func)(stat, msg));
}

typedef int (*fn_status_set_link_code_func)(FN_status_t *,
	unsigned int code);

extern "C" int
fn_status_set_link_code(FN_status_t *arg0, unsigned int code)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_set_link_code", &func);

	return (((fn_status_set_link_code_func)func)(arg0, code));
}

typedef int (*fn_status_set_link_remaining_name_func)(
	FN_status_t *,
	const FN_composite_name_t *);

extern "C" int
fn_status_set_link_remaining_name(
	FN_status_t *arg0,
	const FN_composite_name_t *arg1)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_set_link_remaining_name", &func);

	return (((fn_status_set_link_remaining_name_func)func)(arg0, arg1));
}

typedef int (*fn_status_set_link_resolved_name_func)(
	FN_status_t *,
	const FN_composite_name_t *);

extern "C" int
fn_status_set_link_resolved_name(
	FN_status_t *arg0,
	const FN_composite_name_t *arg1)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_set_link_resolved_name", &func);

	return (((fn_status_set_link_resolved_name_func)func)(arg0, arg1));
}

typedef int (*fn_status_set_link_resolved_ref_func)(FN_status_t *,
	const FN_ref_t *);

extern "C" int
fn_status_set_link_resolved_ref(FN_status_t *arg0,  const FN_ref_t *arg1)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_set_link_resolved_ref", &func);

	return (((fn_status_set_link_resolved_ref_func)func)(arg0, arg1));
}

typedef int (*fn_status_append_resolved_name_func)(
	FN_status_t *,
	const FN_composite_name_t *);

extern "C" int
fn_status_append_resolved_name(
	FN_status_t *arg0,
	const FN_composite_name_t *arg1)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_append_resolved_name", &func);

	return (((fn_status_append_resolved_name_func)func)(arg0, arg1));
}

typedef int (*fn_status_append_remaining_name_func)(
	FN_status_t *,
	const FN_composite_name_t *);

extern "C" int
fn_status_append_remaining_name(
	FN_status_t *arg0,
	const FN_composite_name_t *arg1)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_append_remaining_name", &func);

	return (((fn_status_append_remaining_name_func)func)(arg0, arg1));
}

typedef int (*fn_status_set_link_diagnostic_message_func)(
	FN_status_t *stat,
	const FN_string_t *msg);

extern "C" int
fn_status_set_link_diagnostic_message(FN_status_t *stat,
	const FN_string_t *msg)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_set_link_diagnostic_message", &func);

	return (((fn_status_set_link_diagnostic_message_func)func)(stat, msg));
}

typedef int (*fn_status_advance_by_name_func)(
	FN_status_t *,
	const FN_composite_name_t *prefix,
	const FN_ref_t *resolved_ref);

extern "C" int
fn_status_advance_by_name(
	FN_status_t *arg0, const FN_composite_name_t *prefix,
	const FN_ref_t *resolved_ref)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_advance_by_name", &func);

	return (((fn_status_advance_by_name_func)func)(arg0, prefix,
	    resolved_ref));
}

typedef FN_string_t *(*fn_status_description_func)(
	const FN_status_t *,
	unsigned int detail,
	unsigned int *more_detail);

extern "C" FN_string_t *
fn_status_description(
	const FN_status_t *arg0, unsigned int detail,
	unsigned int *more_detail)
{
	static void *func = 0;

	fn_locked_dlsym("fn_status_description", &func);

	return (((fn_status_description_func)func)(arg0, detail, more_detail));
}
