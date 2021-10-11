/*
 * Copyright (c) 1986-1991 by Sun Microsystems Inc.
 */

#ident	"@(#)authdes_prot.c	1.9	97/04/29 SMI"

#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)authdes_prot.c 1.1 89/03/08 Copyr 1986 Sun Micro";
#endif
/*
 * authdes_prot.c, XDR routines for DES authentication
 *
 */

#include <sys/types.h>
#include <rpc/types.h>
#include <rpc/trace.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/auth_des.h>

#define	ATTEMPT(xdr_op) if (!(xdr_op)) return (FALSE)

bool_t
xdr_authdes_cred(XDR *xdrs, struct authdes_cred *cred)
{
	/*
	 * Unrolled xdr
	 */
	trace1(TR_xdr_authdes_cred, 0);
	ATTEMPT(xdr_enum(xdrs, (enum_t *)&cred->adc_namekind));
	switch (cred->adc_namekind) {
	case ADN_FULLNAME:
		ATTEMPT(xdr_string(xdrs, &cred->adc_fullname.name,
				MAXNETNAMELEN));
		ATTEMPT(xdr_opaque(xdrs, (caddr_t)&cred->adc_fullname.key,
				(u_int)sizeof (des_block)));
		ATTEMPT(xdr_opaque(xdrs, (caddr_t)&cred->adc_fullname.window,
				(u_int)sizeof (cred->adc_fullname.window)));
		trace1(TR_xdr_authdes_cred, 1);
		return (TRUE);
	case ADN_NICKNAME:
		ATTEMPT(xdr_opaque(xdrs, (caddr_t)&cred->adc_nickname,
				(u_int)sizeof (cred->adc_nickname)));
		trace1(TR_xdr_authdes_cred, 1);
		return (TRUE);
	default:
		trace1(TR_xdr_authdes_cred, 1);
		return (FALSE);
	}
}


bool_t
xdr_authdes_verf(register XDR *xdrs, register struct authdes_verf *verf)
{
	/*
	 * Unrolled xdr
	 */
	trace1(TR_xdr_authdes_verf, 0);
	ATTEMPT(xdr_opaque(xdrs, (caddr_t)&verf->adv_xtimestamp,
				(u_int)sizeof (des_block)));
	ATTEMPT(xdr_opaque(xdrs, (caddr_t)&verf->adv_int_u,
				(u_int)sizeof (verf->adv_int_u)));
	trace1(TR_xdr_authdes_verf, 1);
	return (TRUE);
}
