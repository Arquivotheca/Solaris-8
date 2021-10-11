/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FAKE_XFN_HH
#define	_FAKE_XFN_HH

#pragma ident	"@(#)fake_xfn.hh	1.1	96/03/31 SMI"

#include <stddef.h>

typedef struct _FN_attribute FN_attribute_t;

typedef struct _FN_attrmodlist FN_attrmodlist_t;

typedef struct _FN_attrset FN_attrset_t;

typedef struct {
	size_t	length;
	void	*contents;
} FN_attrvalue_t;

typedef struct _FN_composite_name FN_composite_name_t;

typedef struct _FN_compound_name FN_compound_name_t;

typedef struct _FN_ctx FN_ctx_t;

typedef struct _FN_namelist_t FN_namelist_t;

typedef struct _FN_bindinglist_t FN_bindinglist_t;

typedef struct _FN_valuelist_t FN_valuelist_t;

typedef struct _FN_multigetlist_t FN_multigetlist_t;

typedef struct {
	unsigned int	format;
	size_t		length;
	void		*contents;
} FN_identifier_t;

typedef struct _FN_ref FN_ref_t;

typedef struct _FN_ref_addr FN_ref_addr_t;

typedef struct _FN_status FN_status_t;

typedef struct _FN_string FN_string_t;

#ifdef __cplusplus
extern "C" {
#endif

extern void
fn_locked_dlsym(const char *func_name, void **func);

#ifdef __cplusplus
}
#endif

#endif /* _FAKE_XFN_HH */
