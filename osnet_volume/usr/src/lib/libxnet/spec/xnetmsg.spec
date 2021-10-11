#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)xnetmsg.spec	1.1	99/01/25 SMI"
#
# lib/libxnet/spec/xnetmsg.spec
#
# NOTE:
# The CAE specification permits reference to several symbols without
# including <sys/socket.h>, which redefines them to __xnet_<symbol>.
#

#
# Sockets (X/Open CAE Specification (1994), page 6):
#

function	recvmsg extends libsocket/spec/xpgmsg.spec __xnet_recvmsg
version		SUNW_1.1
end		

function	__xnet_recvmsg
weak		recvmsg
version		SUNW_1.1
end		

function	sendmsg extends libsocket/spec/xpgmsg.spec __xnet_sendmsg
version		SUNW_1.1
end		

function	__xnet_sendmsg
weak		sendmsg
version		SUNW_1.1
end		
