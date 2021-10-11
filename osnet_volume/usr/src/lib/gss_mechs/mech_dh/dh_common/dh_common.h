/*
 *	dh_common.h
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef _DH_COMMON_H_
#define	_DH_COMMON_H_

#pragma ident	"@(#)dh_common.h	1.2	98/05/26 SMI"

#include <rpcsvc/nis_dhext.h>

#ifdef __cplusplus
extern "C" {
#endif

gss_mechanism
__dh_generic_initialize(gss_mechanism dhmech,
			gss_OID_desc mech_type, dh_keyopts_t keyopts);

void
__generic_gen_dhkeys(int keylen, char *xmodulus, int proot,
		    char *public, char *secret, char *pass);
void
__generic_common_dhkeys(char *pkey, char *skey, int keylen,
			char *xmodulus, des_block keys[], int keynum);


extern void
des_setparity(char *);

#ifdef __cplusplus
}
#endif

#endif /* _DH_COMMON_H_ */
