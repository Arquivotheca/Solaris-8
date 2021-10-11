/*
 * Copyright (c) 1993 - 1994 by Sun Microsystems, Inc.
 */

#ifndef _XFN_FN_BINDINGSET_H
#define	_XFN_FN_BINDINGSET_H

#pragma ident	"@(#)FN_bindingset.h	1.4	94/11/24 SMI"

#include <xfn/FN_string.h>
#include <xfn/FN_ref.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _FN_bindingset FN_bindingset_t;

extern FN_bindingset_t *fn_bindingset_create(void);
extern void fn_bindingset_destroy(FN_bindingset_t *);

extern FN_bindingset_t *fn_bindingset_copy(const FN_bindingset_t *);
extern FN_bindingset_t *fn_bindingset_assign(
		FN_bindingset_t *dst,
		const FN_bindingset_t *src);

extern unsigned int fn_bindingset_count(const FN_bindingset_t *);

extern const FN_ref_t *fn_bindingset_get_ref(
		const FN_bindingset_t *,
		const FN_string_t *name);

extern const FN_string_t *fn_bindingset_first(
		const FN_bindingset_t *,
		void **iter_pos,
		const FN_ref_t **first_ref);
extern const FN_string_t *fn_bindingset_next(
		const FN_bindingset_t *,
		void **iter_pos,
		const FN_ref_t **next_ref);

extern int fn_bindingset_add(
		FN_bindingset_t *,
		const FN_string_t *name,
		const FN_ref_t *,
		unsigned int exclusive);
extern int fn_bindingset_remove(FN_bindingset_t *, const FN_string_t *name);

#ifdef __cplusplus
}
#endif

#endif /* _XFN_FN_BINDINGSET_H */
