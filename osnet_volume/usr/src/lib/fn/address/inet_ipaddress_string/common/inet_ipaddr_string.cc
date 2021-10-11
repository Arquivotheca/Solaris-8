/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)inet_ipaddr_string.cc	1.6	97/11/12 SMI"


#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <xfn/xfn.hh>

extern FN_string *
fns_default_address_description(const FN_ref_addr &,
				unsigned detail,
				unsigned *more_detail,
				const FN_string *msg);

// - detail identifies the desired level of detail.
// - more_detail, if 0, is ignored.  if non-zero, it
//   is set to specify the next level of detail available
//   (beyond the level specified in 'detail').  if no more
//   detail is available (beyond 'detail'), it will be set
//   to 'detail'.
// For ip addresses,
//	(detail)	0	1	>1
// --------------------------------------------------------------------------
// address type		X	X	X
// ip address		X	X	X (dump format)
// address length		X	X
//

static const
FN_identifier inet_ipaddr_string_type((unsigned char *)"inet_ipaddr_string");

extern "C"
FN_string_t *
Dinet_ipaddr_string(const FN_ref_addr_t *addrt,
    unsigned detail, unsigned *more_detail)
{
	const FN_ref_addr addr = *(const FN_ref_addr *)addrt;
	const FN_identifier *tp = addr.type();
	const void *contents = addr.data();
	const int contents_size = addr.length();

	int is_ipaddr = (*tp == inet_ipaddr_string_type);
	if (!is_ipaddr || contents == 0 || detail > 1) {
		if (more_detail)
			*more_detail = detail;
		char *msg = 0;
		if (!is_ipaddr)
			msg = "Not an IP address string";
		else if (contents == 0)
			msg = "Cannot decode ip address";

		if (msg) {
			FN_string msg_str((unsigned char *)msg);
			return ((FN_string_t *)
			    fns_default_address_description(addr, 2, 0,
			    &msg_str));
		} else {
			return ((FN_string_t *)
			    fns_default_address_description(addr, 2, 0, 0));
		}
	}

	int size = 60; // overhead
	const unsigned char *addr_type_str = 0;
	if (tp) {
		size += tp->length(); // type string
		addr_type_str = tp->str();
	}
	if (contents)
		size += contents_size;
	char *buf = new char[size];
	char *bp;

	sprintf(buf, "Address type: %s\n",
		addr_type_str ? (char *)addr_type_str : "");

	bp = buf + strlen(buf);

	if (detail > 0) {
                sprintf(bp, "  length: %ld\n", addr.length());
		bp = buf + strlen(buf);
	}

	strcat(bp, "  ip address:");
	bp = buf + strlen(buf);

	strncat(bp, (const char *) contents, contents_size);
	bp[contents_size] = '\n';
	bp[contents_size+1] = '\0';

	FN_string *ans2 = new FN_string((unsigned char *)buf);
	delete[] buf;

	if (more_detail) {
		*more_detail = detail+1;
	}

	return ((FN_string_t *)ans2);
}
