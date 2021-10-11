/*
 * Copyright (c) 1993 - 1994 by Sun Microsystems, Inc.
 */

#ifndef _XFN_FN_REF_H
#define	_XFN_FN_REF_H

#pragma ident	"@(#)FN_ref.h	1.3	94/11/20 SMI"

#include <xfn/FN_string.h>
#include <xfn/FN_ref_addr.h>
#include <xfn/FN_composite_name.h>
#include <xfn/FN_identifier.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _FN_ref FN_ref_t;

/*
 * Ops on references.
 */

extern FN_ref_t *fn_ref_create(const FN_identifier_t *ref_type);
extern void fn_ref_destroy(FN_ref_t *ref);
extern FN_ref_t *fn_ref_copy(const FN_ref_t *ref);
extern FN_ref_t *fn_ref_assign(FN_ref_t *dst, const FN_ref_t *src);

extern const FN_identifier_t *fn_ref_type(const FN_ref_t *ref);
extern unsigned int fn_ref_addrcount(const FN_ref_t *ref);

extern const FN_ref_addr_t *fn_ref_first(
		const FN_ref_t *ref,
		void **iter_pos);
extern const FN_ref_addr_t *fn_ref_next(
		const FN_ref_t *ref,
		void **iter_pos);
extern int fn_ref_prepend_addr(FN_ref_t *ref, const FN_ref_addr_t *addr);
extern int fn_ref_append_addr(FN_ref_t *ref, const FN_ref_addr_t *addr);
extern int fn_ref_insert_addr(
		FN_ref_t *ref,
		void **iter_pos,
		const FN_ref_addr_t *addr);
extern int fn_ref_delete_addr(FN_ref_t *ref, void **iter_pos);
extern int fn_ref_delete_all(FN_ref_t *ref);

extern FN_ref_t *fn_ref_create_link(const FN_composite_name_t *link_name);
extern int fn_ref_is_link(const FN_ref_t *ref);

extern FN_composite_name_t *fn_ref_link_name(const FN_ref_t *link_ref);

extern FN_string_t *fn_ref_description(
		const FN_ref_t *ref,
		unsigned int detail,
		unsigned int *more_detail);

#ifdef __cplusplus
}
#endif

#endif /* _XFN_FN_REF_H */
