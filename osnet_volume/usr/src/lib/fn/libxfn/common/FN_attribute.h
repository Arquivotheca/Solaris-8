/*
 * Copyright (c) 1993 - 1994 by Sun Microsystems, Inc.
 */

#ifndef _XFN_FN_ATTRIBUTE_H
#define	_XFN_FN_ATTRIBUTE_H

#pragma ident	"@(#)FN_attribute.h	1.3	94/11/20 SMI"

#include <xfn/FN_attrvalue.h>
#include <xfn/FN_identifier.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _FN_attribute FN_attribute_t;

extern FN_attribute_t *fn_attribute_create(
		const FN_identifier_t *attr_id,
		const FN_identifier_t *attr_syntax);
extern void fn_attribute_destroy(FN_attribute_t *);

extern FN_attribute_t *fn_attribute_copy(const FN_attribute_t *);
extern FN_attribute_t *fn_attribute_assign(
		FN_attribute_t *dst,
		const FN_attribute_t *src);

extern const FN_identifier_t *fn_attribute_identifier(
		const FN_attribute_t *);
extern const FN_identifier_t *fn_attribute_syntax(const FN_attribute_t *);
extern unsigned int fn_attribute_valuecount(const FN_attribute_t *);

extern const FN_attrvalue_t *fn_attribute_first(
		const FN_attribute_t *,
		void **iter_pos);
extern const FN_attrvalue_t *fn_attribute_next(
		const FN_attribute_t *,
		void **iter_pos);

extern int fn_attribute_add(
		FN_attribute_t *,
		const FN_attrvalue_t *,
		unsigned int exclusive);
extern int fn_attribute_remove(
		FN_attribute_t *,
		const FN_attrvalue_t *);

#ifdef __cplusplus
}
#endif

#endif /* _XFN_FN_ATTRIBUTE_H */
