/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)description.cc	1.5	97/11/12 SMI"

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <xfn/xfn.hh>
#include <rpc/types.h>
#include <rpc/xdr.h>

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
// address contents	X	X	X (dump format)
// address length		X	X
//
//
// Address contains
//	NIS+ root domain name
//	server name
//	(optional) server address

static const
FN_identifier onc_fn_nis_root_type((unsigned char *)"onc_fn_nis_root");

static char *
__decode_buffer(const char *encoded_buf, int esize)
{
	char *decoded_buf = 0;
	XDR xdrs;
	bool_t status;

	xdrmem_create(&xdrs, (caddr_t) encoded_buf, esize, XDR_DECODE);
	status = xdr_wrapstring(&xdrs, &decoded_buf);

	if (status == FALSE) {
		xdr_destroy(&xdrs);
		return (0);
	}

	xdr_destroy(&xdrs);
	return (decoded_buf);
}

extern "C"
FN_string_t *
Donc_fn_nis_root(const FN_ref_addr_t *addrt,
    unsigned detail, unsigned *more_detail)
{
	const FN_ref_addr addr = *(const FN_ref_addr *)addrt;
	const FN_identifier *tp = addr.type();
	const char *contents = (char *)addr.data();
	const int contents_size = addr.length();

	int is_okaddr = (*tp == onc_fn_nis_root_type);
	if (!is_okaddr || contents == 0 || detail > 1) {
		if (more_detail)
			*more_detail = detail;
		char *msg = 0;
		if (!is_okaddr)
			msg = "Address type is not 'onc_fn_nis_root'";
		else if (contents == 0)
			msg = "Cannot decode address";

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
		size += (contents_size+30);
	char *buf = new char[size];
	char *bp;

	sprintf(buf, "Address type: %s\n",
		addr_type_str ? (char *)addr_type_str : "");

	bp = buf + strlen(buf);

	if (detail > 0) {
                sprintf(bp, "  length: %ld\n", addr.length());

		bp = buf + strlen(buf);
	}


	// Decode address
	char *mycontents = __decode_buffer((const char *)contents,
					    contents_size);
	char *token;

	strcat(bp, "  NIS domain: ");
	char *ptr;
	if (token = strtok_r(mycontents, " \t\n", &ptr))
		strcat(bp, token);
	strcat(bp, "\n");

	strcat(bp, "  server name: ");
	if (token = strtok_r(NULL, " \t\n", &ptr))
		strcat(bp, token);
	strcat(bp, "\n");

	if (token = strtok_r(NULL, " \t\n", &ptr)) {
		// address has server address
		strcat(bp, " server address: ");
		strcat(bp, token);
		strcat(bp, "\n");
	}

	free(mycontents);

	FN_string *ans2 = new FN_string((unsigned char *)buf);
	delete[] buf;

	if (more_detail) {
		*more_detail = detail+1;
	}

	return ((FN_string_t *)ans2);
}
