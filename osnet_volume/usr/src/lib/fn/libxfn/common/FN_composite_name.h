/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_FN_COMPOSITE_NAME_H
#define	_XFN_FN_COMPOSITE_NAME_H

#pragma ident	"@(#)FN_composite_name.h	1.4	96/03/31 SMI"

#include <xfn/FN_string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _FN_composite_name FN_composite_name_t;

extern FN_composite_name_t *fn_composite_name_create(void);
extern void fn_composite_name_destroy(FN_composite_name_t *);
extern FN_composite_name_t *fn_composite_name_from_string(
		const FN_string_t *);
extern FN_composite_name_t *fn_composite_name_from_str(
		const unsigned char *);
extern FN_string_t *fn_string_from_composite_name(
		const FN_composite_name_t *,
		unsigned int *status);

extern FN_composite_name_t *fn_composite_name_copy(
		const FN_composite_name_t *);
extern FN_composite_name_t *fn_composite_name_assign(
		FN_composite_name_t *dst,
		const FN_composite_name_t *src);
extern int fn_composite_name_is_empty(const FN_composite_name_t *);
extern unsigned int fn_composite_name_count(const FN_composite_name_t *);

extern const FN_string_t *fn_composite_name_first(
		const FN_composite_name_t *,
		void **iter_pos);
extern const FN_string_t *fn_composite_name_next(
		const FN_composite_name_t *,
		void **iter_pos);
extern const FN_string_t *fn_composite_name_prev(
		const FN_composite_name_t *,
		void **iter_pos);
extern const FN_string_t *fn_composite_name_last(
		const FN_composite_name_t *,
		void **iter_pos);

extern FN_composite_name_t *fn_composite_name_prefix(
		const FN_composite_name_t *,
		const void *iter_pos);
extern FN_composite_name_t *fn_composite_name_suffix(
		const FN_composite_name_t *,
		const void *iter_pos);

extern int fn_composite_name_is_equal(
		const FN_composite_name_t *n1,
		const FN_composite_name_t *n2,
		unsigned int *status);

extern int fn_composite_name_is_prefix(
		const FN_composite_name_t *,
		const FN_composite_name_t *prefix,
		void **iter_pos,
		unsigned int *status);
extern int fn_composite_name_is_suffix(
		const FN_composite_name_t *,
		const FN_composite_name_t *suffix,
		void **iter_pos,
		unsigned int *status);

extern int fn_composite_name_prepend_comp(
		FN_composite_name_t *,
		const FN_string_t *);
extern int fn_composite_name_append_comp(
		FN_composite_name_t *,
		const FN_string_t *);

extern int fn_composite_name_insert_comp(
		FN_composite_name_t *,
		void **iter_pos,
		const FN_string_t *);
extern int fn_composite_name_delete_comp(
		FN_composite_name_t *,
		void **iter_pos);

extern int fn_composite_name_prepend_name(
		FN_composite_name_t *,
		const FN_composite_name_t *);
extern int fn_composite_name_append_name(
		FN_composite_name_t *,
		const FN_composite_name_t *);

extern int fn_composite_name_insert_name(
		FN_composite_name_t *,
		void **iter_pos,
		const FN_composite_name_t *);

#ifdef __cplusplus
}
#endif

#endif /* _XFN_FN_COMPOSITE_NAME_H */
