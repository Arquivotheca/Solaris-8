/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_FN_SEARCH_FILTER_H
#define	_XFN_FN_SEARCH_FILTER_H

#pragma ident	"@(#)FN_search_filter.h	1.1	96/03/31 SMI"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum FN_search_filter_type {
	FN_SEARCH_FILTER_ATTR = 1,
	FN_SEARCH_FILTER_ATTRVALUE,
	FN_SEARCH_FILTER_STRING,
	FN_SEARCH_FILTER_IDENTIFIER
	};
typedef enum FN_search_filter_type FN_search_filter_type;

typedef struct _FN_search_filter FN_search_filter_t;

extern FN_search_filter_t *prelim_fn_search_filter_create(
	unsigned int *status,
	const unsigned char *estr,
	...);

extern void prelim_fn_search_filter_destroy(FN_search_filter_t *sfilter);

extern FN_search_filter_t *prelim_fn_search_filter_copy(
	const FN_search_filter_t *sfilter);

extern FN_search_filter_t *prelim_fn_search_filter_assign(
	FN_search_filter_t *dst,
	const FN_search_filter_t *src);

extern const unsigned char *prelim_fn_search_filter_expression(
	const FN_search_filter_t *sfilter);

extern const void **prelim_fn_search_filter_arguments(
	const FN_search_filter_t *sfilter,
	size_t *number_of_arguments);

#ifdef __cplusplus
}
#endif

#endif /* _XFN_FN_SEARCH_FILTER_H */
