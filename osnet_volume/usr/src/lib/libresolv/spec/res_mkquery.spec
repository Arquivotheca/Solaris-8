#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)res_mkquery.spec	1.1	99/01/25 SMI"
#
# lib/libresolv/spec/res_mkquery.spec

function	res_mkquery
include		<sys/types.h>, <struct_rrec_compat.h>
declaration	int res_mkquery(int op, const char *dname, int class, \
			int type, const char *data, int datalen, \
			struct rrec *newrr, uchar_t *buf, int buflen )
version		SUNW_0.7
exception	$return == -1
end		

function	_res
version		SUNW_0.7
end		

# nss_dns.so.1
function	__res_set_no_hosts_fallback
version		SUNWprivate_1.1
end		

# in.named
function	_res_opcodes
version		SUNWprivate_1.1
end		

# in.named
function	_res_resultcodes
version		SUNWprivate_1.1
end		

# nss_dns.so.1
function	res_endhostent
version		SUNWprivate_1.1
end		

# nss_dns.so.1
function	res_gethostbyaddr
version		SUNWprivate_1.1
end		

# nss_dns.so.1m hotjava
function	res_gethostbyname
version		SUNWprivate_1.1
end		

# nss_dns.so.1
function	res_sethostent
version		SUNWprivate_1.1
end		
