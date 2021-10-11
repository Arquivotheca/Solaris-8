#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)xnet.spec	1.1	99/01/25 SMI"
#
# lib/libxnet/spec/xnet.spec
#
# NOTE:
# The CAE specification permits reference to several symbols without
# including <sys/socket.h>, which redefines them to __xnet_<symbol>.
#

#
# Sockets (X/Open CAE Specification (1994), page 6):
#

function	accept extends libsocket/spec/socket.spec accept
version		SUNW_1.1
end		

function	bind extends libsocket/spec/socket.spec __xnet_bind
version		SUNW_1.1
end		

function	__xnet_bind
weak		bind
version		SUNW_1.1
end		

function	connect extends libsocket/spec/socket.spec __xnet_connect
version		SUNW_1.1
end		

function	__xnet_connect
weak		connect
version		SUNW_1.1
end		

function	getpeername extends libsocket/spec/socket.spec
version		SUNW_1.1
end		

function	getsockname extends libsocket/spec/socket.spec
version		SUNW_1.1
end		

function	getsockopt extends libsocket/spec/socket.spec __xnet_getsockopt
version		SUNW_1.1
end		

function	__xnet_getsockopt
weak		getsockopt
version		SUNW_1.1
end		

function	listen extends libsocket/spec/socket.spec __xnet_listen
version		SUNW_1.1
end		

function	__xnet_listen
weak		listen
version		SUNW_1.1
end		

function	recv extends libsocket/spec/socket.spec recv
version		SUNW_1.1
end		

function	recvfrom extends libsocket/spec/socket.spec recvfrom
version		SUNW_1.1
end		

function	send extends libsocket/spec/socket.spec send
version		SUNW_1.1
end		

function	sendto extends libsocket/spec/socket.spec sendto
version		SUNW_1.1
end		

function	setsockopt extends libsocket/spec/socket.spec setsockopt
version		SUNW_1.1
end		

function	shutdown extends libsocket/spec/socket.spec shutdown
version		SUNW_1.1
end		

function	socket extends libsocket/spec/socket.spec __xnet_socket
version		SUNW_1.1
end		

function	__xnet_socket
weak		socket
version		SUNW_1.1
end		

function	socketpair extends libsocket/spec/socket.spec __xnet_socketpair
version		SUNW_1.1
end		

function	__xnet_socketpair
weak		socketpair
version		SUNW_1.1
end		

#
# IP Address Resolution: (X/Open CAE Specification (1994), page 6)
#
function	endhostent extends libnsl/spec/nsl.spec
version		SUNW_1.1
end		

function	getprotoent extends libsocket/spec/socket.spec
version		SUNW_1.1
end		

function	inet_addr extends libnsl/spec/inet.spec
version		SUNW_1.1
end		

function	ntohs extends libsocket/spec/socket.spec
version		SUNW_1.1
end		

function	endnetent extends libsocket/spec/socket.spec
version		SUNW_1.1
end		

function	gethostname extends libc/spec/gen.spec
version		SUNW_1.1
end		

function	getservbyname extends libsocket/spec/socket.spec
version		SUNW_1.1
end		

function	inet_lnaof extends libsocket/spec/socket.spec
version		SUNW_1.1
end		

function	sethostent extends libnsl/spec/nsl.spec
version		SUNW_1.1
end		

function	endprotoent extends libsocket/spec/socket.spec
version		SUNW_1.1
end		

function	getnetbyaddr extends libsocket/spec/socket.spec
version		SUNW_1.1
end		

function	getservbyport extends libsocket/spec/socket.spec
version		SUNW_1.1
end		

function	inet_makeaddr extends libsocket/spec/socket.spec
version		SUNW_1.1
end		

function	setnetent extends libsocket/spec/socket.spec
version		SUNW_1.1
end		

function	endservent extends libsocket/spec/socket.spec
version		SUNW_1.1
end		

function	getnetbyname extends libsocket/spec/socket.spec
version		SUNW_1.1
end		

function	getservent extends libsocket/spec/socket.spec
version		SUNW_1.1
end		

function	inet_netof extends libnsl/spec/inet.spec
version		SUNW_1.1
end		

function	setprotoent extends libsocket/spec/socket.spec
version		SUNW_1.1
end		

function	gethostbyaddr extends libnsl/spec/nsl.spec
version		SUNW_1.1
end		

function	getnetent extends libsocket/spec/socket.spec
version		SUNW_1.1
end		

data		h_errno		# defined in data.c
version		SUNW_1.1
end		

function	inet_network extends libsocket/spec/socket.spec
version		SUNW_1.1
end		

function	setservent extends libsocket/spec/socket.spec
version		SUNW_1.1
end		

function	gethostbyname extends libnsl/spec/nsl.spec
version		SUNW_1.1
end		

function	getprotobyname extends libsocket/spec/socket.spec
version		SUNW_1.1
end		

function	htonl extends libsocket/spec/socket.spec
version		SUNW_1.1
end		

function	inet_ntoa extends libnsl/spec/inet.spec
version		SUNW_1.1
end		

function	gethostent extends libnsl/spec/nsl.spec
version		SUNW_1.1
end		

function	getprotobynumber extends libsocket/spec/socket.spec
version		SUNW_1.1
end		

function	htons extends libsocket/spec/socket.spec
version		SUNW_1.1
end		

function	ntohl extends libsocket/spec/socket.spec
version		SUNW_1.1
end		

function	__t_errno
declaration	int *__t_errno(void)
version		SUNW_1.1
end		

function	__xnet_sendto	extends libsocket/spec/socket.spec
version		SUNW_1.1
end		

#
# X/Open Networking Services, Issue 4, page 6 XTI interfaces
#
function	_xti_accept extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_alloc extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_bind extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_close extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_connect extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_error extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_free extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_getinfo extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_getprotaddr extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_getstate extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_listen extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_look extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_open extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_optmgmt extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_rcv extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_rcvconnect extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_rcvdis extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_rcvrel extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_rcvudata extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_rcvuderr extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_snd extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_snddis extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_sndrel extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_sndudata extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_strerror extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_sync extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

function	_xti_unbind extends libnsl/spec/xti.spec
version		SUNW_1.1
end		

data		t_errno		# defined in data.c
version		SUNW_1.1
end		

#
# X/Open Networking Services (Issue 5)
#
function	_xti_rcvreldata extends libnsl/spec/xti.spec
version		SUNW_1.2
end		

function	_xti_rcvv extends libnsl/spec/xti.spec
version		SUNW_1.2
end		

function	_xti_rcvvudata extends libnsl/spec/xti.spec
version		SUNW_1.2
end		

function	_xti_sndreldata extends libnsl/spec/xti.spec
version		SUNW_1.2
end		

function	_xti_sndv extends libnsl/spec/xti.spec
version		SUNW_1.2
end		

function	_xti_sndvudata extends libnsl/spec/xti.spec
version		SUNW_1.2
end		

function	_xti_sysconf extends libnsl/spec/xti.spec
version		SUNW_1.2
end		

function	_xti_xns5_accept extends libnsl/spec/xti.spec
version		SUNW_1.2
end		

function	_xti_xns5_snd extends libnsl/spec/xti.spec
version		SUNW_1.2
end		

