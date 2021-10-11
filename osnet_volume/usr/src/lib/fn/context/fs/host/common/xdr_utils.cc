/*
 * Copyright (c) 1994, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)xdr_utils.cc	1.1	94/12/05 SMI"


#include <stdlib.h>
#include <string.h>
#include <xfn/xfn.hh>
#include <rpc/rpc.h>
#include "xdr_utils.hh"


static const FN_identifier reftype((unsigned char *)"onc_fn_fs");
static const FN_identifier addrtype((unsigned char *)"onc_fn_fs_host");


// Create and return an address containing the XDR-encoding of the
// given string.  Return NULL on error.

static FN_ref_addr *addr_from_str(const char *addrdata);


// Return the XDR-decoded string in the given address.  Return NULL on error.

static char *str_from_addr(const FN_ref_addr *);


FN_ref *
fs_host_encode_ref(const char *hostname, const FN_composite_name *dir)
{
	FN_string *string = dir->string();
	if (string == NULL) {
		return (NULL);
	}
	const char *directory = (const char *)string->str();
	if (directory == NULL) {
		delete string;
		return (NULL);
	}
	size_t len = strlen(hostname) + 2 + strlen(directory);
	FN_ref_addr *addr;
	char *addrdata = new char[len + 1];
	if (addrdata != NULL) {
		sprintf(addrdata, "%s:/%s", hostname, directory);
		addr = addr_from_str(addrdata);
		delete[] addrdata;
	}
	delete string;
	if (addr == NULL) {
		return (NULL);
	}
	FN_ref *ref = new FN_ref(reftype);
	if (ref != NULL) {
		if (ref->append_addr(*addr) == 0) {
			delete ref;
			ref = NULL;
		}
	}
	delete addr;
	return (ref);
}


int
fs_host_decode_addr(const FN_ref_addr &addr, FN_composite_name *&dir,
    char *&hostname)
{
	char *addrdata = str_from_addr(&addr);
	if (addrdata == NULL) {
		return (0);
	}
	char *directory;
	char *colon = strchr(addrdata, ':');
	if (colon == NULL) {
		directory = "/";	// root directory
	} else {
		*colon = '\0';
		directory = colon + 1;
	}
	hostname = new char[strlen(addrdata) + 1];
	if (hostname == NULL) {
		delete[] addrdata;
		return (0);
	}
	strcpy(hostname, addrdata);

	// Skip leading '/', which would add an extra empty component
	// to the beginning of dir.
	if (directory[0] == '/') {
		directory++;
	}
	dir = new FN_composite_name((unsigned char *)directory);
	delete[] addrdata;
	return (dir != NULL);
}


static FN_ref_addr *
addr_from_str(const char *addrdata)
{
	char buf[ADDRESS_SIZE];
	XDR xdr;
	FN_ref_addr *addr = NULL;
	xdrmem_create(&xdr, (caddr_t)buf, sizeof (buf), XDR_ENCODE);
	if (xdr_string(&xdr, (char **)&addrdata, ~0)) {
		addr = new FN_ref_addr(addrtype, xdr_getpos(&xdr), buf);
	}
	xdr_destroy(&xdr);
	return (addr);
}


static char *
str_from_addr(const FN_ref_addr *addr)
{
	XDR xdr;
	char *str = NULL;
	xdrmem_create(&xdr, (caddr_t)addr->data(), addr->length(), XDR_DECODE);
	if (!xdr_string(&xdr, &str, ~0)) {
		str = NULL;
	}
	xdr_destroy(&xdr);
	return (str);
}
