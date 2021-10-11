/*
 * Copyright (c) 1993 - 1994 by Sun Microsystems, Inc.
 */

#ifndef _XFN_FN_COMPOUND_NAME_H
#define	_XFN_FN_COMPOUND_NAME_H

#pragma ident	"@(#)FN_compound_name.h	1.3	94/11/20 SMI"

/*
 * Declarations for compound names.
 */

#include <xfn/FN_string.h>
#include <xfn/FN_ctx.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _FN_compound_name FN_compound_name_t;

extern FN_compound_name_t *fn_compound_name_from_syntax_attrs(
		const FN_attrset_t *aset,
		const FN_string_t *name,
		FN_status_t *status);

extern FN_attrset_t *fn_compound_name_get_syntax_attrs(
		const FN_compound_name_t *name);

extern void fn_compound_name_destroy(FN_compound_name_t *name);

extern FN_string_t *fn_string_from_compound_name(
		const FN_compound_name_t *name);

extern FN_compound_name_t *fn_compound_name_copy(
		const FN_compound_name_t *name);

extern FN_compound_name_t *fn_compound_name_assign(
		FN_compound_name_t *dst,
		const FN_compound_name_t *src);

extern unsigned int fn_compound_name_count(const FN_compound_name_t *name);

extern const FN_string_t *fn_compound_name_first(
		const FN_compound_name_t *name,
		void **iter_pos);
extern const FN_string_t *fn_compound_name_next(
		const FN_compound_name_t *name,
		void **iter_pos);
extern const FN_string_t *fn_compound_name_prev(
		const FN_compound_name_t *name,
		void **iter_pos);
extern const FN_string_t *fn_compound_name_last(
		const FN_compound_name_t *name,
		void **iter_pos);

extern FN_compound_name_t *fn_compound_name_prefix(
		const FN_compound_name_t *name,
		const void *iter_pos);
extern FN_compound_name_t *fn_compound_name_suffix(
		const FN_compound_name_t *name,
		const void *iter_pos);

extern int fn_compound_name_is_empty(const FN_compound_name_t *name);
extern int fn_compound_name_is_equal(
		const FN_compound_name_t *name,
		const FN_compound_name_t *,
		unsigned int *status);
extern int fn_compound_name_is_prefix(
		const FN_compound_name_t *name,
		const FN_compound_name_t *prefix,
		void **iter_pos,
		unsigned int *status);
extern int fn_compound_name_is_suffix(
		const FN_compound_name_t *name,
		const FN_compound_name_t *suffix,
		void **iter_pos,
		unsigned int *status);

extern int fn_compound_name_prepend_comp(
		FN_compound_name_t *name,
		const FN_string_t *atomic_comp,
		unsigned int *status);
extern int fn_compound_name_append_comp(
		FN_compound_name_t *name,
		const FN_string_t *atomic_comp,
		unsigned int *status);

extern int fn_compound_name_insert_comp(
		FN_compound_name_t *name,
		void **iter_pos,
		const FN_string_t *atomic_comp,
		unsigned int *status);
extern int fn_compound_name_delete_comp(
		FN_compound_name_t *name,
		void **iter_pos);

extern int fn_compound_name_delete_all(FN_compound_name_t *name);

#ifdef __cplusplus
}
#endif

#endif /* _XFN_FN_COMPOUND_NAME_H */
