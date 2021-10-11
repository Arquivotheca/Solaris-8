/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 */

#ifndef _XFN_FN_SEARCH_CONTROL_H
#define	_XFN_FN_SEARCH_CONTROL_H

#pragma ident	"@(#)FN_search_control.h	1.1	96/03/31 SMI"

#include <xfn/FN_attrset.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Search Scopes */
enum {
	FN_SEARCH_ONE_CONTEXT = 0,
	FN_SEARCH_NAMED_OBJECT = 1,
	FN_SEARCH_SUBTREE = 2,
	FN_SEARCH_CONSTRAINED_SUBTREE = 3
};

typedef struct _FN_search_control FN_search_control_t;

extern FN_search_control_t *prelim_fn_search_control_create(
	unsigned int scope,
	unsigned int follow_links,
	unsigned int max_names,
	unsigned int return_ref,
	const FN_attrset_t *return_attr_ids,
	unsigned int *status);

extern void prelim_fn_search_control_destroy(
	FN_search_control_t *scontrol);

extern FN_search_control_t *prelim_fn_search_control_copy(
	const FN_search_control_t *scontrol);

extern FN_search_control_t *prelim_fn_search_control_assign(
	FN_search_control_t *dst,
	const FN_search_control_t *src);

extern unsigned int prelim_fn_search_control_scope(
	const FN_search_control_t *scontrol);

extern unsigned int prelim_fn_search_control_follow_links(
	const FN_search_control_t *scontrol);

extern unsigned int prelim_fn_search_control_max_names(
	const FN_search_control_t *scontrol);

extern unsigned int prelim_fn_search_control_return_ref(
	const FN_search_control_t *scontrol);

extern const FN_attrset_t *prelim_fn_search_control_return_attr_ids(
	const FN_search_control_t *scontrol);

#ifdef __cplusplus
}
#endif

#endif /* _XFN_FN_SEARCH_CONTROL_H */
