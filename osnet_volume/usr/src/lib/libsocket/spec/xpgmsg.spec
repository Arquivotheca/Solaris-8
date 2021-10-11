#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)xpgmsg.spec	1.1	99/01/25 SMI"
#

function	__xnet_recvmsg
include		"xpgmsg_spec.h", <sys/types.h>, <sys/socket.h>
declaration	ssize_t __xnet_recvmsg(int socket, struct XPG_msghdr *msg, \
			int flags)
version		SUNW_1.1
errno		EBADF ENOTSOCK EINVAL EWOULDBLOCK EAGAIN EINTR EOPNOTSUPP \
			ENOTCONN ETIMEDOUT ECONNRESET EIO ENOBUFS ENOMEM ENOSR
exception	$return == -1
end

function	__xnet_sendmsg
include		"xpgmsg_spec.h", <sys/types.h>, <sys/socket.h>
declaration	ssize_t __xnet_sendmsg(int socket, const struct XPG_msghdr *msg, \
			int flags )
version		SUNW_1.1
errno		EAFNOSUPPORT EBADF ECONNRESET EINTR EINVAL EMSGSIZE ENOTCONN \
			ENOTSOCK EOPNOTSUPP EPIPE EWOULDBLOCK EAGAIN EACCES EIO \
			ELOOP ENAMETOOLONG ENOENT ENOTDIR EDESTADDRREQ EHOSTUNREACH \
			EISCONN ENETDOWN ENETUNREACH ENOBUFS ENOSR
exception	$return == -1
end
