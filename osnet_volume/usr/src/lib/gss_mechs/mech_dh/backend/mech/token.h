/*
 *	token.h
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 */

#ifndef _TOKEN_H_
#define	_TOKEN_H_

#pragma ident	"@(#)token.h	1.2	98/05/26 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include "dh_gssapi.h"
#include "dhmech_prot.h"

OM_uint32
__make_ap_token(gss_buffer_t, gss_OID, dh_token_t, dh_key_set_t);

OM_uint32
__make_token(gss_buffer_t, gss_buffer_t, dh_token_t, dh_key_set_t);

OM_uint32
__get_ap_token(gss_buffer_t, gss_OID, dh_token_t, dh_signature_t);

OM_uint32
__get_token(gss_buffer_t, gss_buffer_t, dh_token_t, dh_key_set_t);

#ifdef __cplusplus
}
#endif

#endif /* _TOKEN_H_ */
