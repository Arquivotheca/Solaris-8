/*
 * Copyright (c) 1993 - 1994 by Sun Microsystems, Inc.
 */

#ifndef _XFN_FN_ATTRSET_H
#define	_XFN_FN_ATTRSET_H

#pragma ident	"@(#)FN_attrset.h	1.3	94/11/20 SMI"

#include <xfn/FN_ref.h>		/* to get FN_identifier */
#include <xfn/FN_attribute.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _FN_attrset FN_attrset_t;

extern FN_attrset_t *fn_attrset_create(void);
extern void fn_attrset_destroy(FN_attrset_t *);

extern FN_attrset_t *fn_attrset_copy(const FN_attrset_t *);
extern FN_attrset_t *fn_attrset_assign(
		FN_attrset_t *dst,
		const FN_attrset_t *src);

extern const FN_attribute_t *fn_attrset_get(
		const FN_attrset_t *,
		const FN_identifier_t *attr);

extern unsigned int fn_attrset_count(const FN_attrset_t *);

extern const FN_attribute_t *fn_attrset_first(
		const FN_attrset_t *,
		void **iter_pos);
extern const FN_attribute_t *fn_attrset_next(
		const FN_attrset_t *,
		void **iter_pos);

extern int fn_attrset_add(
		FN_attrset_t *,
		const FN_attribute_t *attr,
		unsigned int exclusive);
extern int fn_attrset_remove(
		FN_attrset_t *,
		const FN_identifier_t *attr);

#ifdef __cplusplus
}
#endif

#endif /* _XFN_FN_ATTRSET_H */
