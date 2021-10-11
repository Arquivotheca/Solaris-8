#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)netdir.spec	1.3	99/05/14 SMI"
#
# lib/libnsl/spec/netdir.spec

function	netdir_getbyname
include		<netdir.h>
declaration	int netdir_getbyname(struct netconfig  *config, \
			struct nd_hostserv *service, \
			struct nd_addrlist **addrs)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
end

function	netdir_getbyaddr
include		<netdir.h>
declaration	int netdir_getbyaddr(struct netconfig  *config, \
			struct nd_hostservlist **service, \
			struct netbuf  *netaddr)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
end

function	netdir_free
include		<netdir.h>
declaration	void netdir_free(void *ptr, const int struct_type)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
end

function	netdir_options
include		<netdir.h>
declaration	int netdir_options(struct netconfig *config, \
			int option, int fildes, char *point_to_args)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
exception	$return == -1
end

function	taddr2uaddr
include		<netdir.h>
declaration	char *taddr2uaddr(struct netconfig *config, struct netbuf *addr)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
exception	$return == 0
end

function	uaddr2taddr
include		<netdir.h>
declaration	struct netbuf *uaddr2taddr(struct netconfig *config, \
			char *uaddr)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
exception	$return == 0
end

function	netdir_perror
include		<netdir.h>
declaration	void netdir_perror(char *s)
version		SUNW_0.7
end

function	netdir_sperror
include		<netdir.h>
declaration	char *netdir_sperror(void)
version		SUNW_0.7
exception	$return == 0
end

