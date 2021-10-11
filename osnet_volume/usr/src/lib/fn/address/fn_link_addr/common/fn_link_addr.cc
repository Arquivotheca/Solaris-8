/*
 * Copyright (c) 1992 - 1994 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)fn_link_addr.cc	1.6	97/11/12 SMI"


#include <xfn/xfn.hh>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

extern FN_string *
fns_default_address_description(const FN_ref_addr &,
				unsigned detail,
				unsigned *more_detail,
				const FN_string *msg);


// detail identifies the desired level of detail.
//
// more_detail, if 0, is ignored.  if non-zero, it
// is set to specify the next level of detail available
// (beyond the level specified in 'detail').  if no more
// detail is available (beyond 'detail'), it will be set to 'detail'.

// detail		0	>0
// ----------------------------------
// address type		X	X
// composite_name	X	X (dump)
// address length		X
//

static const
FN_identifier fn_link_addr_type((unsigned char *)"fn_link_addr");

extern "C"
FN_string_t *
Dfn_link_addr(const FN_ref_addr_t *addrt,
    unsigned detail, unsigned *more_detail)
{
	const FN_ref_addr addr = *(const FN_ref_addr *)addrt;
	const FN_identifier *tp = addr.type();
	const void *contents = addr.data();
	const size_t contents_size = addr.length();

	int is_linkaddr = (*tp == fn_link_addr_type);
	if (!is_linkaddr || contents == 0 || detail > 1) {
		if (more_detail)
			*more_detail = detail;
		char *msg = 0;
		if (!is_linkaddr)
			msg = "Not an XFN link address";
		else if (contents == 0)
			msg = "Cannot decode XFN link address";

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

	size_t size = 60; // overhead
	if (tp)
		size += tp->length(); // type string
	if (contents)
		size += contents_size;
	char *buf = new char[size];
	char *bp;

	sprintf(buf, "Address type: %s\n", tp? (char *)(tp->str()) : "");
	bp = buf + strlen(buf);

	if (detail > 0) {
		sprintf(bp, "  length: %ld\n", addr.length());
		bp = buf + strlen(buf);
	}

	strcat(bp, "  Link name: ");
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
