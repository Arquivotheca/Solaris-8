#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)rpcsoc.spec 1.1	99/09/20 SMI"
#

function	clnttcp_create
include		<sys/types.h>
include		<rpc/rpc.h>
declaration	CLIENT *clnttcp_create(struct sockaddr_in *raddr, \
			rpcprog_t prog, rpcvers_t vers, \
			int *sockp, uint_t sendsz, uint_t recvsz)
version		SUNW_0.7
end

function	clntudp_bufcreate
include		<sys/types.h>
include		<rpc/rpc.h>
include		<sys/time.h>
declaration	CLIENT *clntudp_bufcreate(struct sockaddr_in *raddr, \
			rpcprog_t program, \
			rpcvers_t version, \
			struct timeval wait, \
			int *sockp, \
			uint_t sendsz, \
			uint_t recvsz)
version		SUNW_0.7
end

function	clntudp_create
include		<sys/types.h>
include		<rpc/rpc.h>
include		<sys/time.h>
declaration	CLIENT *clntudp_create(struct sockaddr_in *raddr, \
			rpcprog_t program, \
			rpcvers_t version, \
			struct timeval wait, \
			int *sockp)
version		SUNW_0.7
end

function	get_myaddress
include		<sys/socket.h>
declaration	void get_myaddress(struct sockaddr_in *addr)
version		SUNW_0.7
end

function	getrpcport
include		<rpc/rpc.h>
include		<sys/socket.h>
declaration	ushort_t getrpcport(char *host, \
			rpcprog_t prognum, \
			rpcvers_t versnum, \
			rpcprot_t proto)
version		SUNW_0.7
end

function	rtime
include 	<sys/socket.h>
include		<sys/time.h>
declaration	int rtime(struct sockaddr_in *addrp, \
			struct timeval *timep, struct timeval *timeout)
version		SUNW_0.7
end

function	svcfd_create
include		<rpc/rpc.h>
declaration	SVCXPRT *svcfd_create(int fd, uint_t sendsize, uint_t recvsize)
version		SUNW_0.7
end

function	svctcp_create
include		<rpc/rpc.h>
declaration	SVCXPRT *svctcp_create(int sock, \
			uint_t sendsize, uint_t recvsize)
version		SUNW_0.7
end

function	svcudp_bufcreate
include		<rpc/rpc.h>
declaration	SVCXPRT *svcudp_bufcreate(int sock, \
			uint_t sendsz, uint_t recvsz)
version		SUNW_0.7
end

function	svcudp_create
include		<rpc/rpc.h>
declaration	SVCXPRT *svcudp_create(int sock)
version		SUNW_0.7
end

function	svcudp_enablecache
include		<rpc/rpc.h>
declaration	int svcudp_enablecache(SVCXPRT *transp, uint_t size)
version		SUNW_0.7
end
