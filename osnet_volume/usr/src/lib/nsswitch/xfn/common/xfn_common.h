/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_COMMON_H
#define	_XFN_COMMON_H

#pragma ident	"@(#)xfn_common.h	1.1	99/01/21 SMI"

#include <nss_dbdefs.h>
#include <xfn/xfn.h>

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * Construct the static initializer for an identifier, given its name as
 * a string literal.
 */
#define	IDENTIFIER(name)	{FN_ID_STRING, sizeof (name) - 1, (name)}


typedef struct xfn_backend	*xfn_backend_ptr_t;
typedef nss_status_t	(*xfn_backend_op_t)(xfn_backend_ptr_t, void *args);

extern nss_backend_t	*_nss_xfn_constr(xfn_backend_op_t ops[], int n_ops);
extern nss_status_t	_nss_xfn_destr(xfn_backend_ptr_t, void *dummy);

/*
 * Map an XFN status code into an nsswitch status code.
 */
extern nss_status_t	_nss_xfn_map_statcode(unsigned int);

#ifdef	__cplusplus
}
#endif

#endif	/* _XFN_COMMON_H */
