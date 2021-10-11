/*
 * Copyright (c) 1994-1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)osi_paddr.cc	1.2	96/03/31 SMI"


#include <xfn/xfn.hh>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

extern FN_string	*fns_default_address_description(const FN_ref_addr &,
				unsigned detail, unsigned *more_detail,
				const FN_string *msg);

// For OSI presentation addresses:
//
// Information		detail
//			0	1	2	>2
// ------------------------------------------------
// address type		X	X	X
// p-selector		X	X	X
// s-selector		X	X	X
// t-selector		X	X	X
// n-address(es)	X	X	X
// address length		X	X
// address data			X	X
// dump						X

static const FN_identifier	osi_paddr_type((unsigned char *)"osi_paddr");

extern "C" {
FN_string_t *
Dosi_paddr(
	const FN_ref_addr_t	*addrt,
	unsigned		detail,
	unsigned		*more_detail
)
{
	const FN_ref_addr	addr = *(const FN_ref_addr *)addrt;
	const FN_identifier	*tp = addr.type();
	const void*		contents = addr.data();
	const int		contents_size = addr.length();
	int			is_paddr = (*tp == osi_paddr_type);

#ifdef DEBUG
	char			time_string[32];
	const time_t		time_value = time((time_t *)0);
	cftime(time_string, "%c", &time_value);
	fprintf(stderr, "%s (fns x500) osi_paddr__fn_ref_addr_description()\n",
	    time_string);
#endif

	if (!is_paddr || contents == 0 || detail > 2) {
		if (more_detail)
			*more_detail = detail;

		char	*msg = 0;
		if (!is_paddr)
			msg = "not an OSI presentation address";
		else if (contents == 0)
			msg = "empty presentation address";

		if (msg) {
			FN_string	msg_str((unsigned char *)msg);
			return ((FN_string_t *)fns_default_address_description
			    (addr, 2, 0, &msg_str));
		} else {
			return ((FN_string_t *)fns_default_address_description
			    (addr, 2, 0, 0));
		}
	}

	int	size = 256; // overhead

	if (tp)
		size += tp->length(); // type string
	if (contents)
		size += contents_size;

	char	*buf = new char[size];
	char	*bp;

	sprintf(buf, "Address type: %s\n", tp ? (char *)(tp->str()) : "");
	bp = buf + strlen(buf);

	sprintf(bp, "  OSI Presentation Address: %s\n", contents);
	bp = buf + strlen(buf);

	if (detail > 1) {
		sprintf(bp, "  address length: %d\n", contents_size);
		bp = buf + strlen(buf);
		sprintf(bp, "  address data: %s\n\n", contents);
		bp = buf + strlen(buf);
		*bp = '\0';
	}

	FN_string	*ans2 = new FN_string((unsigned char *)buf);
	delete [] buf;

	if (more_detail) {
		*more_detail = detail + 1;
	}

	return ((FN_string_t *)ans2);
}
};
