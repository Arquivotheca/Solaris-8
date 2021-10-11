/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#ifndef _XFN_FN_NAMESET_H
#define	_XFN_FN_NAMESET_H

#pragma ident	"@(#)FN_nameset.h	1.3	94/11/20 SMI"

#include <xfn/FN_string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _FN_nameset FN_nameset_t;

extern FN_nameset_t *fn_nameset_create(void);
extern void fn_nameset_destroy(FN_nameset_t *);

extern FN_nameset_t *fn_nameset_copy(const FN_nameset_t *);
extern FN_nameset_t *fn_nameset_assign(
		FN_nameset_t *dst,
		const FN_nameset_t *src);

extern unsigned int fn_nameset_count(const FN_nameset_t *);

extern const FN_string_t *fn_nameset_first(
		const FN_nameset_t *,
		void **iter_pos);
extern const FN_string_t *fn_nameset_next(
		const FN_nameset_t *,
		void **iter_pos);

extern int fn_nameset_add(
		FN_nameset_t *,
		const FN_string_t *name,
		unsigned int exclusive);
extern int fn_nameset_remove(FN_nameset_t *, const FN_string_t *name);

#ifdef __cplusplus
}
#endif

#endif /* _XFN_FN_NAMESET_H */
