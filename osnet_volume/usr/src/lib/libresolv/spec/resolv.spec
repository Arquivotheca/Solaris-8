#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)resolv.spec	1.2	99/05/14 SMI"
#
# lib/libresolv/spec/resolv.spec

function	_getlong
version		SUNW_0.7
end

function	_getshort
version		SUNW_0.7
end

function	res_querydomain
version		SUNW_0.7
end

function	res_init
include		<sys/types.h>, <netinet/in.h>, <arpa/nameser.h>, <resolv.h>
declaration	int res_init(void)
version		SUNW_0.7
end

function	res_query
include		<sys/types.h>, <netinet/in.h>, <arpa/nameser.h>, <resolv.h>
declaration	int res_query(const char *dname, int class, int type, \
			u_char *answer, int anslen)
version		SUNW_0.7
end

function	res_search
include		<sys/types.h>, <netinet/in.h>, <arpa/nameser.h>, <resolv.h>
declaration	int res_search(const char *dname, int class, int type, \
			u_char *answer, int anslen)
version		SUNW_0.7
end

function	res_send
include		<sys/types.h>, <netinet/in.h>, <arpa/nameser.h>, <resolv.h>
declaration	int res_send(const uchar_t *msg, int msglen, uchar_t *answer, \
			int anslen)
version		SUNW_0.7
exception	$return == -1
end

function	dn_comp
include		<sys/types.h>, <netinet/in.h>, <arpa/nameser.h>, <resolv.h>
declaration	int dn_comp(const char *exp_dn, u_char *comp_dn, \
			int length, u_char **dnptrs, u_char **lastdnptr)
version		SUNW_0.7
exception	$return == -1
end

function	dn_expand
include		<sys/types.h>, <netinet/in.h>, <arpa/nameser.h>, <resolv.h>
declaration	int dn_expand(const uchar_t *msg, const uchar_t *eomorig, \
			const uchar_t *comp_dn, char *exp_dn, int length)
version		SUNW_0.7
exception	$return == -1
end

function	strcasecmp extends libc/spec/gen.spec strcasecmp
version		SUNW_0.7
end

function	strncasecmp extends libc/spec/gen.spec strncasecmp
version		SUNW_0.7
end

function	dn_skipname
version		SUNW_0.7
end

function	fp_query
version		SUNW_0.7
end

function	h_errno
version		SUNW_0.7
end

function	hostalias
version		SUNW_0.7
end

function	p_cdname
version		SUNW_0.7
end

function	p_class
version		SUNW_0.7
end

function	p_query
version		SUNW_0.7
end

function	p_rr
version		SUNW_0.7
end

function	p_time
version		SUNW_0.7
end

function	p_type
version		SUNW_0.7
end

function	putlong
version		SUNW_0.7
end
