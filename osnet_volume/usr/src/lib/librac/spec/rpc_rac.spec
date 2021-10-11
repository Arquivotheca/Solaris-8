#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)rpc_rac.spec	1.1	99/01/25 SMI"
#
# lib/librac/spec/rpc_rac.spec

function	rac_drop
include		<rpc/rpc.h>, <rpc/rac.h>
declaration	void rac_drop(CLIENT *cl, void *h )
version		SUNW_0.7
end		

function	rac_poll
include		<rpc/rpc.h>, <rpc/rac.h>
declaration	enum clnt_stat rac_poll(CLIENT *cl, void *h )
version		SUNW_0.7
exception	$return != RPC_SUCCESS
end		

function	rac_recv
include		<rpc/rpc.h>, <rpc/rac.h>
declaration	enum clnt_stat rac_recv(CLIENT *cl, void *h )
version		SUNW_0.7
exception	$return != RPC_SUCCESS
end		

function	rac_send
include		<rpc/rpc.h>, <rpc/rac.h>
declaration	void  *rac_send(CLIENT *cl, rpcproc_t proc, xdrproc_t  xargs, void *argsp, xdrproc_t xresults, void *resultsp, struct timeval timeout)
version		SUNW_0.7
exception	$return == 0
end		

function	__rpc_control
version		SUNWprivate_1.1 
end		

function	__rpc_dtbsize
version		SUNWprivate_1.1 
end		

function	__rpc_endconf
version		SUNWprivate_1.1 
end		

function	__rpc_get_a_size
version		SUNWprivate_1.1 
end		

function	__rpc_get_t_size
version		SUNWprivate_1.1 
end		

function	__rpc_getconf
version		SUNWprivate_1.1 
end		

function	__rpc_getconfip
version		SUNWprivate_1.1 
end		

function	__rpc_select_to_poll
version		SUNWprivate_1.1 
end		

function	__rpc_setconf
version		SUNWprivate_1.1 
end		

function	__rpc_timeval_to_msec
version		SUNWprivate_1.1 
end		

function	__seterr_reply
version		SUNWprivate_1.1 
end		

function	_rpctypelist
version		SUNWprivate_1.1 
end		

function	clnt_create
version		SUNW_0.7
end		

function	clnt_create_vers
version		SUNW_0.7
end		

function	clnt_dg_create
version		SUNW_0.7
end		

function	clnt_tli_create
version		SUNW_0.7
end		

function	clnt_tp_create
version		SUNW_0.7
end		

function	clnt_vc_create
version		SUNW_0.7
end		

function	rac_senderr
version		SUNW_0.7
end		

function	rpcb_getaddr
version		SUNW_0.7
end		

function	rpcb_getmaps
version		SUNW_0.7
end		

function	rpcb_gettime
version		SUNW_0.7
end		

function	rpcb_rmtcall
version		SUNW_0.7
end		

function	rpcb_set
version		SUNW_0.7
end		

function	rpcb_taddr2uaddr
version		SUNW_0.7
end		

function	rpcb_uaddr2taddr
version		SUNW_0.7
end		

function	rpcb_unset
version		SUNW_0.7
end		

function	xdrrec_create
version		SUNW_0.7
end		

function	xdrrec_endofrecord
version		SUNW_0.7
end		

function	xdrrec_eof
version		SUNW_0.7
end		

function	xdrrec_readbytes
version		SUNW_0.7
end		

function	xdrrec_skiprecord
version		SUNW_0.7
end		

