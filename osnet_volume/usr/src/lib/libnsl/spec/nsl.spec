#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)nsl.spec	1.4	99/05/14 SMI"
#
# lib/libnsl/spec/nsl.spec

# NOTE: Look at "versions" file for more details on why there may
# appear to be "gaps" in version number space.

function	endhostent
include		<netdb.h>
declaration	int endhostent(void)
version		SUNW_0.7
end

function	doconfig
include		<sac.h>
declaration	int doconfig(int fildes, char *script, long rflag)
version		SUNW_0.7
end

function	freehostent
include		<sys/socket.h>, <netdb.h>
declaration	void freehostent(struct hostent *hent)
version		SUNW_1.7
end

function	gethostbyname
include		<netdb.h>
declaration	struct hostent *gethostbyname(const char *name)
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
exception	$return == 0
end

function	gethostbyname_r
include		<netdb.h>
declaration	struct hostent *gethostbyname_r(const char *name, \
			struct hostent *result, char *buffer, \
			int buflen, int *h_errnop)
version		SUNW_0.7
exception	$return == 0
end

function	gethostbyaddr
include		<netdb.h>
declaration	struct hostent *gethostbyaddr(const char *addr, \
			int len, int type)
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
exception	$return == 0
end

function	gethostbyaddr_r
include		<netdb.h>
declaration	struct hostent *gethostbyaddr_r(const char *addr, \
			int length, int type, struct hostent *result, \
			char *buffer, int buflen, int *h_errnop)
version		SUNW_0.7
exception	$return == 0
end

function	gethostent
include		<netdb.h>
declaration	struct hostent *gethostent(void)
version		SUNW_0.7
exception	$return == 0
end

function	gethostent_r
include		<netdb.h>
declaration	struct hostent *gethostent_r(struct hostent *result, \
			char *buffer, int buflen, int *h_errnop)
version		SUNW_0.7
exception	$return == 0
end

function	getipnodebyaddr
include		<sys/socket.h>, <netdb.h>
declaration	struct hostent *getipnodebyaddr(const void *src, size_t len, \
			int type, int *error_num)
version		SUNW_1.7
exception	$return == 0
end


function	getipnodebyname
include		<sys/socket.h>, <netdb.h>
declaration	struct hostent *getipnodebyname(const char *name, int af, \
			int flags, int *error_num)
version		SUNW_1.7
exception	$return == 0
end

function	sethostent
include		<netdb.h>
declaration	int sethostent(int stayopen)
version		SUNW_0.7
exception	$return == -1
end

function	gethostname
declaration	int gethostname(char *name, int namelen)
version		SUNWprivate_1.1
errno		EFAULT EPERM
exception	$return == -1
end

function	getnetconfig
include		<netconfig.h>
declaration	struct netconfig *getnetconfig(void *handlep)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
exception	$return == 0
end

function	setnetconfig
include		<netconfig.h>
declaration	void *setnetconfig(void)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
exception	$return == 0
end

function	endnetconfig
include		<netconfig.h>
declaration	int endnetconfig(void *handlep)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
exception	$return == -1
end

function	getnetconfigent
include		<netconfig.h>
declaration	struct netconfig *getnetconfigent(char *netid)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
exception	$return == 0
end

function	freenetconfigent
include		<netconfig.h>
declaration	void freenetconfigent(struct netconfig *netconfigp)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
end

function	nc_perror
include		<netconfig.h>
declaration	void nc_perror(const char *msg)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
end

function	nc_sperror
include		<netconfig.h>
declaration	char *nc_sperror(void)
version		SUNW_0.7
exception	$return == 0
end

function	getnetpath
include		<netconfig.h>
declaration	struct netconfig *getnetpath(void *handlep)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
exception	$return == 0
end

function	setnetpath
include		<netconfig.h>
declaration	void *setnetpath(void)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
exception	$return == 0
end

function	endnetpath
include		<netconfig.h>
declaration	int endnetpath(void *handlep)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
exception	$return == -1
end

function	getpublickey
include		<rpc/rpc.h>, <rpc/key_prot.h>
declaration	int getpublickey(const char *netname, char *publickey)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
exception	$return == 0
end

function	getsecretkey
include		<rpc/rpc.h>, <rpc/key_prot.h>
declaration	int getsecretkey(const char *netname, char *secretkey, \
			const char *passwd)
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
exception	$return == 0
end

function	getrpcbyname
include		<rpc/rpcent.h>
declaration	struct rpcent *getrpcbyname(const char * name)
version		SUNW_0.7
exception	$return == 0
end

function	getrpcbyname_r
include		<rpc/rpcent.h>
declaration	struct rpcent *getrpcbyname_r(const char *name, \
			struct rpcent *result, char *buffer, int buflen)
version		SUNW_0.7
exception	$return == 0
end

function	getrpcbynumber
include		<rpc/rpcent.h>
declaration	struct rpcent *getrpcbynumber(const int number)
version		SUNW_0.7
exception	$return == 0
end

function	getrpcbynumber_r
include		<rpc/rpcent.h>
declaration	struct rpcent *getrpcbynumber_r(const int number, \
			struct rpcent *result, char *buffer, int buflen)
version		SUNW_0.7
exception	$return == 0
end

function	getrpcent
include		<rpc/rpcent.h>
declaration	struct rpcent *getrpcent(void)
version		SUNW_0.7
exception	$return == 0
end

function	getrpcent_r
include		<rpc/rpcent.h>
declaration	struct rpcent *getrpcent_r(struct rpcent *result, \
			char *buffer, int buflen)
version		SUNW_0.7
exception	$return == 0
end

function	setrpcent
include		<rpc/rpcent.h>
declaration	void setrpcent(const int stayopen)
version		SUNW_0.7
end

data		t_nerr
version		SUNW_0.7
end

data		t_errno
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
end

function	__t_errno
declaration	int *__t_errno(void)
version		SUNW_0.7
end

function	t_getname
declaration	int t_getname(int fd, struct netbuf *name, int type)
version		SUNW_0.7
end

function	_nderror
version		sparc=SYSVABI_1.3 i386=SYSVABI_1.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
end

data		h_errno
version		SUNW_0.7
end

function	_null_auth
version		i386=SUNW_0.7 sparc=SISCD_2.3 sparcv9=SUNW_0.7 ia64=SUNW_0.7
end
