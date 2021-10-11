/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)xfn_common.c	1.1	99/01/21 SMI"


#include <stdlib.h>
#include <string.h>
#include <nss_dbdefs.h>
#include <xfn/xfn.h>
#include "xfn_common.h"


struct xfn_backend {
	xfn_backend_op_t	*ops;
	nss_dbop_t		n_ops;
};


/* Syntax to use for attribute creation. */
const FN_identifier_t ascii = IDENTIFIER("fn_attr_syntax_ascii");


nss_status_t
_nss_xfn_map_statcode(unsigned int fn_stat)
{
	switch (fn_stat) {
	case FN_SUCCESS:
		return (NSS_SUCCESS);
	case FN_E_CTX_UNAVAILABLE:
		return (NSS_TRYAGAIN);
	case FN_E_NAME_NOT_FOUND:
	case FN_E_NOT_A_CONTEXT:
	case FN_E_CTX_NO_PERMISSION:
	case FN_E_NO_SUPPORTED_ADDRESS:
	case FN_E_NO_SUCH_ATTRIBUTE:
	case FN_E_ATTR_NO_PERMISSION:
	case FN_E_INVALID_ATTR_IDENTIFIER:
	case FN_E_TOO_MANY_ATTR_VALUES:
	case FN_E_OPERATION_NOT_SUPPORTED:
	case FN_E_PARTIAL_RESULT:
	case FN_E_INCOMPATIBLE_CODE_SETS:
	case FN_E_INCOMPATIBLE_LOCALES:
		return (NSS_NOTFOUND);
	default:
		return (NSS_UNAVAIL);
	}
}


nss_backend_t *
_nss_xfn_constr(xfn_backend_op_t ops[], int n_ops)
{
	xfn_backend_ptr_t be;

	be = (xfn_backend_ptr_t) malloc(sizeof (*be));
	if (be != NULL) {
		be->ops	= ops;
		be->n_ops = n_ops;
	}
	return ((nss_backend_t *)be);
}


nss_status_t
_nss_xfn_destr(xfn_backend_ptr_t be, void *dummy)
{
	free(be);
	return (NSS_SUCCESS);
}
