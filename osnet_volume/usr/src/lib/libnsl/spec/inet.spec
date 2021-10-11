#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)inet.spec	1.4	99/05/14 SMI"
#
# lib/libnsl/spec/inet.spec

# NOTE: Look at "versions" file for more details on why there may 
# appear to be "gaps" in version number space.

function	inet_addr
include		<sys/types.h>, <sys/socket.h>, <netinet/in.h>, <arpa/inet.h>
declaration	in_addr_t inet_addr(const char *cp)
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
end

function	inet_netof
include		<sys/types.h>, <sys/socket.h>, <netinet/in.h>, <arpa/inet.h>
declaration	in_addr_t inet_netof(struct in_addr in)
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
end

function	inet_ntoa
include		<sys/types.h>, <sys/socket.h>, <netinet/in.h>, <arpa/inet.h>
declaration	char *inet_ntoa(const struct in_addr in)
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
exception	$return == 0
end

function	inet_ntoa_r
declaration	char *inet_ntoa_r(struct in_addr in, char *b)
version		SUNW_0.7
end

function	inet_ntop
include		<sys/socket.h>, <arpa/inet.h>
declaration	const char *inet_ntop(int af, const void *src, char *dst, size_t size)
version		SUNW_1.7
exception	$return == 0
end

function	inet_pton
include		<sys/socket.h>, <arpa/inet.h>
declaration	int inet_pton(int af, const char *src, void *dst)
version		SUNW_1.7
exception	$return == -1
end
