/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */


#pragma ident	"@(#)FNSP_nisplus_address.cc	1.2	96/04/04 SMI"

#include "FNSP_nisplus_address.hh"
#include "fnsp_nisplus_root.hh"
#include <rpcsvc/nis.h>


static void
set_access_flags(const FN_string &name,
	unsigned int authoritative, unsigned int &flags)
{
	if (FNSP_home_hierarchy_p(name) == 0)
		flags |= NO_AUTHINFO;

	if (authoritative)
		flags |= MASTER_ONLY;
}

static const FN_string open_bracket((unsigned char *)"[");
static const FN_string close_bracket((unsigned char *)"]");
static const FN_string equal_sign((unsigned char *)"=");
static const FN_string quote_string((unsigned char *)"\"");
static const FN_string comma_string((unsigned char *)",");

static inline int
FNSP_table_name_p(const FN_string &name)
{
	return (name.next_substring(open_bracket) == 0);
}

// If name is of form "[contextname=<index_part>]<table_part>",
// set tab to table_part and ind to index_part.
// Otherwise, set 'tab' to entire 'name'.
static int
decompose_index_name(const FN_string &name, FN_string &tab, FN_string &ind)
{
	if (FNSP_table_name_p(name)) {
		int pos = name.next_substring(close_bracket);
		if (pos > 0) {
			int tabstart = pos + 1;
			int tabend = name.charcount()-1;

			FN_string wholeindpart(name, 1, pos-1);
			pos = wholeindpart.next_substring(equal_sign);
			if (pos > 0) {
				int indstart = pos + 1;
				int indend = wholeindpart.charcount() - 1;
				// get rid of quotes
				if (wholeindpart.compare_substring(indstart,
				    indstart, quote_string) == 0) {
					++indstart;
					--indend;
				}

				ind = FN_string(wholeindpart, indstart, indend);

				// get rid of comma
				if (name.compare_substring(tabstart, tabstart,
				    comma_string) == 0)
					++tabstart;

				tab = FN_string(name, tabstart, tabend);
				return (1);
			}
		}
	}
	// default
	tab = name;
	return (0);
}


FNSP_nisplus_address::~FNSP_nisplus_address()
{
}

void
FNSP_nisplus_address::nisplus_init(unsigned int auth)
{

	switch (get_context_type()) {
	case FNSP_generic_context:
	case FNSP_service_context:
	case FNSP_nsid_context:
	case FNSP_user_context:
	case FNSP_host_context:
	case FNSP_site_context:
	case FNSP_printername_context:
	case FNSP_printer_object:
		if (decompose_index_name(internal_name, table_name, index_name))
			impl_type = FNSP_entries_impl;
		else
			impl_type = FNSP_shared_table_impl;
		break;
	}

	set_access_flags(table_name, auth, access_flags);
}

FNSP_nisplus_address::FNSP_nisplus_address(const FN_ref_addr& ra,
    unsigned int auth)
: FNSP_Address(ra, auth)
{
	nisplus_init(auth);
}

FNSP_nisplus_address::FNSP_nisplus_address(const FN_string& objname,
    unsigned ctype, unsigned rtype, unsigned int auth) :
FNSP_Address(objname, ctype, rtype, auth)
{
	nisplus_init(auth);
}

FNSP_nisplus_address::FNSP_nisplus_address(const FN_ref& r, unsigned int auth)
: FNSP_Address(r, auth)
{
	nisplus_init(auth);
}
