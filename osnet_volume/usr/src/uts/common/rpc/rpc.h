/*
 * Copyright (c) 1986 - 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * rpc.h, Just includes the billions of rpc header files necessary to
 * do remote procedure calling.
 *
 */

#ifndef _RPC_RPC_H
#define	_RPC_RPC_H

#pragma ident	"@(#)rpc.h	1.16	99/07/18 SMI"

/*	rpc.h 1.13 88/12/17 SMI	*/

#include <rpc/types.h>		/* some typedefs */

#ifndef _KERNEL
#include <tiuser.h>
#include <fcntl.h>
#include <memory.h>
#else
#include <sys/tiuser.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <sys/t_kuser.h>
#endif

#include <rpc/xdr.h>		/* generic (de)serializer */
#include <rpc/auth.h>		/* generic authenticator (client side) */
#include <rpc/clnt.h>		/* generic client side rpc */

#include <rpc/rpc_msg.h>	/* protocol for rpc messages */
#include <rpc/auth_sys.h>	/* protocol for unix style cred */
#include <rpc/auth_des.h>	/* protocol for des style cred */
#include <sys/socket.h>		/* generic socket info */
#include <rpc/rpcsec_gss.h>	/* GSS style security */

#include <rpc/svc.h>		/* service manager and multiplexer */
#include <rpc/svc_auth.h>	/* service side authenticator */

#ifndef _KERNEL
#include <rpc/rpcb_clnt.h>	/* rpcbind interface functions */
#include <rpc/svc_mt.h>		/* private server definitions */
#endif

#endif	/* !_RPC_RPC_H */
