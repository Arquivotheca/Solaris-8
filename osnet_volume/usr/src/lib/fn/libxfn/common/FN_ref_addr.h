/*
 * Copyright (c) 1993 - 1994 by Sun Microsystems, Inc.
 */

#ifndef _XFN_FN_REF_ADDR_H
#define	_XFN_FN_REF_ADDR_H

#pragma ident	"@(#)FN_ref_addr.h	1.3	94/11/20 SMI"

#include <xfn/FN_string.h>
#include <xfn/FN_identifier.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _FN_ref_addr FN_ref_addr_t;

/*
 * Ops on reference addresses.
 */

extern FN_ref_addr_t *fn_ref_addr_create(
		const FN_identifier_t *type,
		size_t len,
		const void *data);
extern void fn_ref_addr_destroy(FN_ref_addr_t *addr);

extern FN_ref_addr_t *fn_ref_addr_copy(const FN_ref_addr_t *addr);
extern FN_ref_addr_t *fn_ref_addr_assign(
		FN_ref_addr_t *dst,
		const FN_ref_addr_t *src);

extern const FN_identifier_t *fn_ref_addr_type(const FN_ref_addr_t *addr);
extern size_t fn_ref_addr_length(const FN_ref_addr_t *addr);
extern const void *fn_ref_addr_data(const FN_ref_addr_t *addr);

extern FN_string_t *fn_ref_addr_description(const FN_ref_addr_t *addr,
						unsigned int detail,
						unsigned int *more_detail);

#ifdef __cplusplus
}
#endif

#endif /* _XFN_FN_REF_ADDR_H */
