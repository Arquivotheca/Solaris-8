/*
 * Copyright (c) 1993 - 1994 by Sun Microsystems, Inc.
 */

#ifndef _XFN_FN_ATTRMODLIST_H
#define	_XFN_FN_ATTRMODLIST_H

#pragma ident	"@(#)FN_attrmodlist.h	1.3	94/11/20 SMI"

#include <xfn/FN_attribute.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _FN_attrmodlist FN_attrmodlist_t;

extern FN_attrmodlist_t *fn_attrmodlist_create(void);
extern void fn_attrmodlist_destroy(FN_attrmodlist_t *);

extern FN_attrmodlist_t *fn_attrmodlist_copy(const FN_attrmodlist_t *);
extern FN_attrmodlist_t *fn_attrmodlist_assign(
		FN_attrmodlist_t *dst,
		const FN_attrmodlist_t *src);

extern unsigned int fn_attrmodlist_count(const FN_attrmodlist_t *);

extern const FN_attribute_t *fn_attrmodlist_first(
		const FN_attrmodlist_t *,
		void **iter_pos,
		unsigned int *first_mod_op);
extern const FN_attribute_t *fn_attrmodlist_next(
		const FN_attrmodlist_t *,
		void **iter_pos,
		unsigned int *mod_op);

extern int fn_attrmodlist_add(
		FN_attrmodlist_t *,
		unsigned int mod_op,
		const FN_attribute_t *mod_args);

#ifdef __cplusplus
}
#endif

#endif /* _XFN_FN_ATTRMODLIST_H */
