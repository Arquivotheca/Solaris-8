/*
 * Copyright (c) 1992 - 1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)FNSP_global_entries.cc	1.5	96/03/31 SMI"


#include <xfn/fn_p.hh>

#include "FNSP_entries.hh"

// These are definitions of the subclass specific constructors and
// resolution methods for each type of global entry in
// the initial context.

static const FN_identifier FNSP_global_reftype((unsigned char *)"fn_global");

FN_ref *
FNSP_dns_reference()
{
	static FN_identifier dns_addr_type((unsigned char *)"inet_domain");
	static FN_ref_addr dns_addr(dns_addr_type, 0, 0);

	// construct reference
	FN_ref *r = new FN_ref(FNSP_global_reftype);
	if (r == 0)
		return (0);
	if (r->append_addr(dns_addr) == 0) {
		delete r;
		return (0);
	}
	return (r);
}

FN_ref *
FNSP_x500_reference()
{
	static FN_identifier x500_addr_type((unsigned char *)"x500");
	static FN_ref_addr x500_addr(x500_addr_type, 0, 0);

	// construct reference
	FN_ref *r = new FN_ref(FNSP_global_reftype);
	if (r == 0)
		return (0);
	if (r->append_addr(x500_addr) == 0) {
		delete r;
		return (0);
	}
	return (r);
}

FNSP_InitialContext_GlobalEntry::FNSP_InitialContext_GlobalEntry()
{
	num_names = 1;
	stored_name_type = FNSP_GLOBAL;
	stored_names = new FN_string* [num_names];

	stored_names[0] = new FN_string((unsigned char *)"...");
}

char *FNSP_global_addr_contents = "global";
size_t FNSP_global_addr_contents_size = 6;

void
FNSP_InitialContext_GlobalEntry::resolve(unsigned int /* auth */)
{
	static FN_identifier global_addr_type((unsigned char *)"initial");
	static FN_ref_addr global_addr(global_addr_type,
	    FNSP_global_addr_contents_size,
	    FNSP_global_addr_contents);
	FN_ref *global_ref = new FN_ref(FNSP_global_reftype);

	if (global_ref && global_ref->append_addr(global_addr)) {
		stored_ref = global_ref;
		stored_status_code = FN_SUCCESS;
	} else {
		stored_status_code = FN_E_INSUFFICIENT_RESOURCES;
		if (global_ref)
			delete global_ref;
	}
}

FNSP_InitialContext_GlobalDNSEntry::FNSP_InitialContext_GlobalDNSEntry()
{
	num_names = 1;
	stored_name_type = FNSP_DNS;
	stored_names = new FN_string* [num_names];

	stored_names[0] = new FN_string((unsigned char *)"_dns");
}

void
FNSP_InitialContext_GlobalDNSEntry::resolve(unsigned int /* auth */)
{
	stored_ref = FNSP_dns_reference();
	if (stored_ref)
		stored_status_code = FN_SUCCESS;
	else
		stored_status_code = FN_E_INSUFFICIENT_RESOURCES;
}


FNSP_InitialContext_GlobalX500Entry::FNSP_InitialContext_GlobalX500Entry()
{
	num_names = 1;
	stored_name_type = FNSP_X500;
	stored_names = new FN_string* [num_names];

	stored_names[0] = new FN_string((unsigned char *)"_x500");
}

void
FNSP_InitialContext_GlobalX500Entry::resolve(unsigned int /* auth */)
{
	stored_ref = FNSP_x500_reference();
	if (stored_ref)
		stored_status_code = FN_SUCCESS;
	else
		stored_status_code = FN_E_INSUFFICIENT_RESOURCES;
}
