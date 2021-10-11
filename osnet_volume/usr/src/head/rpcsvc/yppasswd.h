/*
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 *
 */


#ifndef _RPCSVC_YPPASSWD_H
#define	_RPCSVC_YPPASSWD_H

#pragma ident	"@(#)yppasswd.h	1.4	97/08/01 SMI"

#ifndef _PWD_H
#include <pwd.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#define	YPPASSWDPROG ((rpcprog_t)100009)
#define	YPPASSWDVERS ((rpcvers_t)1)
#define	YPPASSWDPROC_UPDATE ((rpcproc_t)1)

struct yppasswd {
	char *oldpass;		/* old (unencrypted) password */
	struct passwd newpw;	/* new pw structure */
};

int xdr_yppasswd();

#ifdef	__cplusplus
}
#endif

#endif	/* !_RPCSVC_YPPASSWD_H */
