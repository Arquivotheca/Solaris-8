/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_FN_STATUS_H
#define	_XFN_FN_STATUS_H

#pragma ident	"@(#)FN_status.h	1.4	96/03/31 SMI"

#include <xfn/FN_ref.h>
#include <xfn/FN_composite_name.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	FN_SUCCESS = 1,
	FN_E_LINK_ERROR,
	FN_E_CONFIGURATION_ERROR,
	FN_E_NAME_NOT_FOUND,
	FN_E_NOT_A_CONTEXT,
	FN_E_LINK_LOOP_LIMIT,
	FN_E_MALFORMED_LINK,
	FN_E_ILLEGAL_NAME,
	FN_E_CTX_NO_PERMISSION,
	FN_E_NAME_IN_USE,
	FN_E_OPERATION_NOT_SUPPORTED,
	FN_E_COMMUNICATION_FAILURE,
	FN_E_CTX_UNAVAILABLE,
	FN_E_NO_SUPPORTED_ADDRESS,
	FN_E_MALFORMED_REFERENCE,
	FN_E_AUTHENTICATION_FAILURE,
	FN_E_INSUFFICIENT_RESOURCES,
	FN_E_CTX_NOT_EMPTY,
	FN_E_NO_SUCH_ATTRIBUTE,
	FN_E_INVALID_ATTR_IDENTIFIER,
	FN_E_INVALID_ATTR_VALUE,
	FN_E_TOO_MANY_ATTR_VALUES,
	FN_E_ATTR_VALUE_REQUIRED,
	FN_E_ATTR_NO_PERMISSION,
	FN_E_PARTIAL_RESULT,
	FN_E_INVALID_ENUM_HANDLE,
	FN_E_SYNTAX_NOT_SUPPORTED,
	FN_E_INVALID_SYNTAX_ATTRS,
	FN_E_INCOMPATIBLE_CODE_SETS,
	FN_E_CONTINUE,
	FN_E_UNSPECIFIED_ERROR,
	FN_E_NO_EQUIVALENT_NAME,
	FN_E_ATTR_IN_USE,
	FN_E_SEARCH_INVALID_FILTER,
	FN_E_SEARCH_INVALID_OP,
	FN_E_SEARCH_INVALID_OPTION,
	FN_E_INCOMPATIBLE_LOCALES
};

typedef struct _FN_status FN_status_t;

extern FN_status_t *fn_status_create(void);
extern void fn_status_destroy(FN_status_t *);
extern FN_status_t *fn_status_copy(const FN_status_t *);
extern FN_status_t *fn_status_assign(
		FN_status_t *dst,
		const FN_status_t *src);
extern unsigned int fn_status_code(const FN_status_t *);
extern const FN_composite_name_t *fn_status_remaining_name(
		const FN_status_t *);
extern const FN_composite_name_t *fn_status_resolved_name(
		const FN_status_t *);
extern const FN_ref_t *fn_status_resolved_ref(const FN_status_t *);

extern const FN_string_t *fn_status_diagnostic_message(
		const FN_status_t *stat);
extern unsigned int fn_status_link_code(const FN_status_t *);
extern const FN_composite_name_t *fn_status_link_remaining_name(
		const FN_status_t *);
extern const FN_composite_name_t *fn_status_link_resolved_name(
		const FN_status_t *);
extern const FN_ref_t *fn_status_link_resolved_ref(const FN_status_t *);

extern const FN_string_t *fn_status_link_diagnostic_message(
		const FN_status_t *stat);
extern int fn_status_is_success(const FN_status_t *);
extern int fn_status_set_success(FN_status_t *);
extern int fn_status_set(
		FN_status_t *,
		unsigned int code,
		const FN_ref_t *resolved_ref,
		const FN_composite_name_t *resolved_name,
		const FN_composite_name_t *remaining_name);
extern int fn_status_set_code(FN_status_t *, unsigned int code);
extern int fn_status_set_remaining_name(
		FN_status_t *,
		const FN_composite_name_t *);
extern int fn_status_set_resolved_name(
		FN_status_t *,
		const FN_composite_name_t *);
extern int fn_status_set_resolved_ref(FN_status_t *, const FN_ref_t *);

extern int fn_status_set_diagnostic_message(
		FN_status_t *stat,
		const FN_string_t *msg);
extern int fn_status_set_link_code(FN_status_t *, unsigned int code);
extern int fn_status_set_link_remaining_name(
		FN_status_t *,
		const FN_composite_name_t *);
extern int fn_status_set_link_resolved_name(
		FN_status_t *,
		const FN_composite_name_t *);
extern int fn_status_set_link_resolved_ref(FN_status_t *, const FN_ref_t *);

extern int fn_status_append_resolved_name(
		FN_status_t *,
		const FN_composite_name_t *);
extern int fn_status_append_remaining_name(
		FN_status_t *,
		const FN_composite_name_t *);

extern int fn_status_set_link_diagnostic_message(
		FN_status_t *stat,
		const FN_string_t *msg);
extern int fn_status_advance_by_name(
		FN_status_t *,
		const FN_composite_name_t *prefix,
		const FN_ref_t *resolved_ref);

extern FN_string_t *fn_status_description(
		const FN_status_t *,
		unsigned int detail,
		unsigned int *more_detail);

#ifdef __cplusplus
}
#endif

#endif /* _XFN_FN_STATUS_H */
