/*
 * Copyright (c) 1996-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_RPC_RPCSYS_H
#define	_RPC_RPCSYS_H

#pragma ident	"@(#)rpcsys.h	1.8	98/06/16 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

enum rpcsys_op  { KRPC_REVAUTH };

/*
 * Private definitions for the krpc_sys/rpcsys system call.
 *
 * flavor_data for AUTH_DES and AUTH_KERB is NULL.
 * flavor_data for RPCSEC_GSS is rpc_gss_OID.
 *
 */
struct krpc_revauth_1 {
	uid_t	uid;
	int	rpcsec_flavor;
	void	*flavor_data;
};

#ifdef _SYSCALL32
struct krpc_revauth_132 {
	uid32_t	uid;
	int32_t	rpcsec_flavor;
	caddr32_t flavor_data;
};
#endif /* _SYSCALL32 */

struct krpc_revauth {
	int	version;	/* initially 1 */
	union	{
		struct krpc_revauth_1 r;
	} krpc_revauth_u;
};
#define	uid_1		krpc_revauth_u.r.uid
#define	rpcsec_flavor_1	krpc_revauth_u.r.rpcsec_flavor
#define	flavor_data_1	krpc_revauth_u.r.flavor_data

#ifdef _SYSCALL32
struct krpc_revauth32 {
	int32_t	version;	/* initially 1 */
	union	{
		struct krpc_revauth_132 r;
	} krpc_revauth_u;
};
#endif /* _SYSCALL32 */


#ifdef _KERNEL

extern	int	rpcsys(enum rpcsys_op opcode, void *arg);
extern	int	sec_clnt_revoke(int, uid_t, cred_t *, void *, model_t);

#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _RPC_RPCSYS_H */
