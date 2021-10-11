#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)svr4msg.spec	1.2	99/05/04 SMI"
#

function	recvmsg
include		"svr4msg_spec.h", <sys/types.h>, <sys/socket.h>
declaration	ssize_t recvmsg(int s, struct SVR4_msghdr *msg, int flags)
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
errno		EBADF EINTR EIO ENOMEM ENOSR ENOTSOCK ESTALE EWOULDBLOCK
exception	$return == -1
end

function	sendmsg
include		"svr4msg_spec.h", <sys/types.h>, <sys/socket.h>
declaration	ssize_t sendmsg(int s, const struct SVR4_msghdr *msg, int flags )
version		i386=SUNW_0.7	sparc=SISCD_2.3	sparcv9=SUNW_0.7 ia64=SUNW_0.7
errno		EBADF EINTR EINVAL EMSGSIZE ENOMEM ENOSR ENOTSOCK EWOULDBLOCK
exception	$return == -1
end

#
# weak interfaces
#
function	_recvmsg
weak		recvmsg
version		SUNWprivate_1.1
end

function	_sendmsg
weak		sendmsg
version		SUNWprivate_1.1
end

function	_socket
weak		socket
version		SUNWprivate_1.1
end		

function	_socketpair
weak		socketpair
version		SUNWprivate_1.1
end		

function	_bind
weak		bind
version		SUNWprivate_1.1
end		

function	_listen
weak		listen
version		SUNWprivate_1.1
end		

function	_accept
weak		accept
version		SUNWprivate_1.1
end		

function	_connect
weak		connect
version		SUNWprivate_1.1
end		

function	_shutdown
weak		shutdown
version		SUNWprivate_1.1
end		

function	_recv
weak		recv
version		SUNWprivate_1.1
end		

function	_recvfrom
weak		recvfrom
version		SUNWprivate_1.1
end		

function	_send
weak		send
version		SUNWprivate_1.1
end		

function	_sendto
weak		sendto
version		SUNWprivate_1.1
end		

function	_getpeername
weak		getpeername
version		SUNWprivate_1.1
end		

function	_getsockname
weak		getsockname
version		SUNWprivate_1.1
end		

function	_getsockopt
weak		getsockopt
version		SUNWprivate_1.1
end		

function	_setsockopt
weak		setsockopt
version		SUNWprivate_1.1
end		

