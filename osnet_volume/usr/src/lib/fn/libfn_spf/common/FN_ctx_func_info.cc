/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FN_ctx_func_info.cc	1.1	96/03/31 SMI"

#include <xfn/xfn.hh>
#include "FN_ctx_func_info.hh"
#include "fn_spi.h"

#define	FN_FOLLOW_LINK	1

FN_ctx_func_info_t *
make_func_info_attr_get(const FN_identifier *i, FN_attribute **answer)
{
	FN_ctx_func_info_t *packet = new FN_ctx_func_info_t;

	packet->opcode = FN_CTX_FUNC_ATTR_GET;
	packet->arg1 = (void *)(i);
	packet->ret = (void **)answer;

	return (packet);
};

FN_ctx_func_info_t *
make_func_info_attr_modify(unsigned int *i, const FN_attribute *a, int *answer)
{
	FN_ctx_func_info_t *packet = new FN_ctx_func_info_t;

	packet->opcode = FN_CTX_FUNC_ATTR_MODIFY;
	packet->arg1 = (void *)(i);
	packet->arg2 = (void *)(a);
	packet->ret = (void **)answer;

	return (packet);
}

FN_ctx_func_info_t *
make_func_info_attr_get_values(const FN_identifier *i, FN_valuelist **answer)
{
	FN_ctx_func_info_t *packet = new FN_ctx_func_info_t;

	packet->opcode = FN_CTX_FUNC_ATTR_GET_VALUES;
	packet->arg1 = (void *)(i);
	packet->ret = (void **)answer;

	return (packet);
};

FN_ctx_func_info_t *
make_func_info_attr_get_ids(FN_attrset **answer)
{
	FN_ctx_func_info_t *packet = new FN_ctx_func_info_t;

	packet->opcode = FN_CTX_FUNC_ATTR_GET_IDS;
	packet->ret = (void **)answer;

	return (packet);
};

FN_ctx_func_info_t *
make_func_info_attr_multi_get(const FN_attrset *a, FN_multigetlist **answer)
{
	FN_ctx_func_info_t *packet = new FN_ctx_func_info_t;

	packet->opcode = FN_CTX_FUNC_ATTR_MULTI_GET;
	packet->arg1 = (void *)(a);
	packet->ret = (void **)answer;

	return (packet);
};


FN_ctx_func_info_t *
make_func_info_attr_multi_modify(const FN_attrmodlist *m,
	FN_attrmodlist **unexec, int *answer)
{
	FN_ctx_func_info_t *packet = new FN_ctx_func_info_t;

	packet->opcode = FN_CTX_FUNC_ATTR_MULTI_MODIFY;
	packet->arg1 = (void *)(m);
	packet->arg2 = (void *)(unexec);
	packet->ret = (void **)answer;

	return (packet);
};


// Perform operation as specified in 'packet' in context 'ctx' using
// 'name' and the arguments specified in 'packet'
// Returns 1 if function executed; 0 otherwise
int
ctx_exec_func(FN_ctx* ctx,
	const FN_composite_name &name,
	FN_status &status,
	FN_ctx_func_info_t *packet)
{
	switch (packet->opcode) {
	case FN_CTX_FUNC_ATTR_GET:
	{
		FN_attribute *get_answer = ctx->attr_get(name,
		    *(const FN_identifier *)(packet->arg1),
		    FN_FOLLOW_LINK,
		    status);
		*(packet->ret) = (void *)get_answer;
		break;
	}
	case FN_CTX_FUNC_ATTR_MODIFY:
	{
		int mod_answer = ctx->attr_modify(name,
		    *(unsigned int *)(packet->arg1),
		    *(const FN_attribute *)(packet->arg2),
		    FN_FOLLOW_LINK,
		    status);
		*(packet->ret) = (void *)mod_answer;
		break;
	}
	case FN_CTX_FUNC_ATTR_GET_VALUES:
	{
		FN_valuelist *get_val_answer = ctx->attr_get_values(name,
		    *(const FN_identifier *)(packet->arg1),
		    FN_FOLLOW_LINK,
		    status);
		*(packet->ret) = (void *)get_val_answer;
		break;
	}
	case FN_CTX_FUNC_ATTR_GET_IDS:
	{
		FN_attrset *get_ids_answer = ctx->attr_get_ids(name,
		    FN_FOLLOW_LINK,
		    status);
		*(packet->ret) = (void *)get_ids_answer;
		break;
	}
	case FN_CTX_FUNC_ATTR_MULTI_GET:
	{
		FN_multigetlist *mg_answer = ctx->attr_multi_get(name,
		    (const FN_attrset *)(packet->arg1),
		    FN_FOLLOW_LINK,
		    status);
		*(packet->ret) = (void *)mg_answer;
		break;
	}
	case FN_CTX_FUNC_ATTR_MULTI_MODIFY:
	{
		int mm_answer = ctx->attr_multi_modify(name,
		    *(const FN_attrmodlist *)(packet->arg1),
		    FN_FOLLOW_LINK,
		    (FN_attrmodlist **)(packet->arg2),
		    status);
		*(packet->ret) = (void *)mm_answer;
		break;
	}
	default:
		status.set(FN_E_OPERATION_NOT_SUPPORTED);
		return (0);
	}

	return (1);
}

int
ctx_svc_exec_func(FN_ctx_svc* ctx,
    const FN_composite_name &name,
    FN_status_psvc &pstatus,
    FN_ctx_func_info_t *packet)
{
	switch (packet->opcode) {
	case FN_CTX_FUNC_ATTR_GET:
	{
		FN_attribute *get_answer = ctx->p_attr_get(name,
		    *((const FN_identifier *)(packet->arg1)),
		    FN_FOLLOW_LINK,
		    pstatus);
		*(packet->ret) = (void *)get_answer;
		break;
	}
	case FN_CTX_FUNC_ATTR_MODIFY:
	{
		int mod_answer = ctx->p_attr_modify(name,
		    *(unsigned int *)(packet->arg1),
		    *(const FN_attribute *)(packet->arg2),
		    FN_FOLLOW_LINK,
		    pstatus);
		*(packet->ret) = (void *)mod_answer;
		break;
	}
	case FN_CTX_FUNC_ATTR_GET_VALUES:
	{
		FN_valuelist *get_val_answer = ctx->p_attr_get_values(name,
		    *((const FN_identifier *)(packet->arg1)),
		    FN_FOLLOW_LINK,
		    pstatus);
		*(packet->ret) = (void *)get_val_answer;
		break;
	}
	case FN_CTX_FUNC_ATTR_GET_IDS:
	{
		FN_attrset *get_ids_answer = ctx->p_attr_get_ids(name,
		    FN_FOLLOW_LINK,
		    pstatus);
		*(packet->ret) = (void *)get_ids_answer;
		break;
	}
	case FN_CTX_FUNC_ATTR_MULTI_GET:
	{
		FN_multigetlist *mg_answer = ctx->p_attr_multi_get(name,
		    (const FN_attrset *)(packet->arg1),
		    FN_FOLLOW_LINK,
		    pstatus);
		*(packet->ret) = (void *)mg_answer;
		break;
	}
	case FN_CTX_FUNC_ATTR_MULTI_MODIFY:
	{
		int mm_answer = ctx->p_attr_multi_modify(name,
		    *((const FN_attrmodlist *)(packet->arg1)),
		    FN_FOLLOW_LINK,
		    (FN_attrmodlist **)(packet->arg2),
		    pstatus);
		*(packet->ret) = (void *)mm_answer;
		break;
	}
	default:
		pstatus.set(FN_E_OPERATION_NOT_SUPPORTED);
		return (0);
	}
	return (1);
}
