/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FN_CTX_FUNC_INFO_HH
#define	_FN_CTX_FUNC_INFO_HH

#pragma ident	"@(#)FN_ctx_func_info.hh	1.1	96/03/31 SMI"

#include <xfn/xfn.hh>
#include <xfn/fn_spi.hh>

typedef struct {
	int opcode;
	void *arg1;
	void *arg2;
	void **ret;
} FN_ctx_func_info_t;

// Constructor functions
extern FN_ctx_func_info_t *
make_func_info_attr_get(const FN_identifier *i, FN_attribute **answer);

extern FN_ctx_func_info_t *
make_func_info_attr_modify(unsigned int *i, const FN_attribute *a, int *answer);

extern FN_ctx_func_info_t *
make_func_info_attr_get_values(const FN_identifier *i, FN_valuelist **answer);

extern FN_ctx_func_info_t *
make_func_info_attr_get_ids(FN_attrset **answer);

extern FN_ctx_func_info_t *
make_func_info_attr_multi_get(const FN_attrset *a, FN_multigetlist **answer);

extern FN_ctx_func_info_t *
make_func_info_attr_multi_modify(const FN_attrmodlist *m,
    FN_attrmodlist **unexec, int *answer);

// Execution functions
extern int
ctx_exec_func(FN_ctx* ctx,
    const FN_composite_name &name,
    FN_status &status,
    FN_ctx_func_info_t *packet);

extern int
ctx_svc_exec_func(FN_ctx_svc* ctx,
    const FN_composite_name &name,
    FN_status_psvc &pstatus,
    FN_ctx_func_info_t *packet);

#endif /* _FN_CTX_FUNC_INFO_HH */
