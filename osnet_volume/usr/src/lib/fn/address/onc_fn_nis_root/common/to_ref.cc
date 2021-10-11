/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)to_ref.cc	1.2	96/04/17 SMI"

#include <xfn/xfn.hh>
#include <xfn/fn_p.hh>
#include <string.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <malloc.h>

// module: fn_inet_nis.so$(VERS)
//
// function: nis__txt_record_to_addrs()
//
// 1 OK
// 0 failed

// Extract address from given TXT records and add the address to 'ref'.
//
// Text records have the following format:
//	TXT <addrtag> <addrinfo>
//
// 'records' is an array of null-terminated strings, each containing
// the contents of the rhs of the TXT record (i.e. <addrinfo>
//
// Text records for an NIS hierarchy has the following format:
//
//	TXT XFNNIS NIS_domain server_name [server_ipaddr]
//
// NIS_domain is the fully qualified name of the NIS domain.
//
// NIS_domain, server_name and server_ipaddr
// are passed to this routine, which uses
// this information to construct an address:
// type:	onc_fn_nis
// contents:	NIS_domain server_name server_ipaddr
//
//
// For example, if the text record is
//
//	TXT XFNNIS Test.COM. alto
//
// 'records' would have 1 string, with contents "Test.COM. alto".
//
// The address added to 'ref' would have
// type:	onc_fn_nis
// contents:	XDR(Test.COM. alto)
//
// Each TXT record (with the correct format) results in one address
// being added to 'ref'

static const FN_identifier
    onc_fn_nis_root_type((unsigned char *)"onc_fn_nis_root");

static char *
__encode_buffer(const char *dstr, int &out_size)
{
	XDR	xdrs;
	char	*encode_buf;
	bool_t 	status;

	// Calculate size and allocate space for result
	int esize = (int) xdr_sizeof((xdrproc_t)xdr_wrapstring, &dstr);
	encode_buf = (char *)malloc(esize);
	if (encode_buf == NULL)
		return (0);

	// XDR structure into buffer
	xdrmem_create(&xdrs, encode_buf, esize, XDR_ENCODE);
	status = xdr_wrapstring(&xdrs, (char **)&dstr);
	if (status == FALSE) {
		xdr_destroy(&xdrs);
		free(encode_buf);
		return (0);
	}
	xdr_destroy(&xdrs);
	out_size = esize;
	return (encode_buf);
}

extern "C" int
nis__txt_records_to_addrs(int num_records, const char **records,
    FN_ref_t *cref, FN_status_t *cstatus)
{
	FN_ref *ref = (FN_ref*)(cref);
	FN_status *status = (FN_status*)(cstatus);

	FN_ref_addr *addr;
	char *addr_contents;
	int i, addr_size;

	status->set_success();

	for (i = 0; i < num_records; i++) {
		// Convert each record type to string, and call the fn_p
		// interface to get the reference type
		addr_contents = __encode_buffer(records[i], addr_size);
		if (addr_contents == 0) {
			// out of memory
			status->set(FN_E_INSUFFICIENT_RESOURCES);
			if (i > 0)
				return (0); // no addresses added
			else
				return (1); // at least 1 was added
		}

		addr = new FN_ref_addr(onc_fn_nis_root_type,
		    addr_size, addr_contents);
		free(addr_contents);

		if (addr) {
			ref->append_addr(*addr);
			delete addr;
		} else {
			// out of memory
			status->set(FN_E_INSUFFICIENT_RESOURCES);
			if (i > 0)
				return (0); // no addresses added
			else
				return (1); // at least 1 was added
		}
	}
	return (1);
}
