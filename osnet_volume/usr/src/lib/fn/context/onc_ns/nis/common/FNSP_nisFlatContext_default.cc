/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_nisFlatContext_default.cc	1.1	96/03/31 SMI"

#include <sys/time.h>

#include "FNSP_nisFlatContext_default.hh"
#include <FNSP_Syntax.hh>
#include <xfn/fn_p.hh>

static const FN_composite_name empty_name((const unsigned char *) "");
static const FN_string FNSP_Service_Name((unsigned char *)"service");
static const FN_string FNSP_Service_Name2((unsigned char *)"_service");
static const FN_string FNSP_Printer_Name((unsigned char *)"printer");

//  A flat naming system is one in which only non-hierarchical names are bound.
//  All names in the naming system are bound in a single context.
//  The only thing that can be bound under a flat name is a nns pointer.
//  This implementation supports junctions.  This means
//  that 'a' and 'a/' resolve to the same thing.
//  This implementation is for files and nis.
//  All bindings are generated algorithmically.  No storage is acctually
//  consulted.
//
//  This is used for the nsid context of org and the service context.
//  The following are the currently supported bindings.  In the future,
//  others (like username and hostname) might also be supported.
//  In an org's nsid context, the following names are bound:
//    service, _service
//  In the service context:
//    printer
//
//  For performance, we do not verify whether the YP domain specified in
//  the reference is reachable.  This is not necessary because we are
//  generating references for bindings and not consulting any YP service
//  for these two types of contexts.

FNSP_nisFlatContext_default::~FNSP_nisFlatContext_default()
{
	delete my_reference;
	delete my_address;
}

// check for null pointers to my_reference and my_address in constructors

#ifdef DEBUG
FNSP_nisFlatContext_default::FNSP_nisFlatContext_default(
    const FN_identifier *addr_type,
    const FN_string &dirname)
{
	my_reference = FNSP_reference(*addr_type, dirname, FNSP_nsid_context);
	my_address = new FNSP_nisAddress(dirname, FNSP_nsid_context);
}

FNSP_nisFlatContext_default::FNSP_nisFlatContext_default(const FN_ref &from_ref)
{
	my_reference = new FN_ref(from_ref);
	my_address = new FNSP_nisAddress(from_ref);
}
#endif

FNSP_nisFlatContext_default::FNSP_nisFlatContext_default(
    const FN_ref_addr &from_addr, const FN_ref& from_ref)
{
	my_reference = new FN_ref(from_ref);
	my_address = new FNSP_nisAddress(from_addr);
}

FNSP_nisFlatContext_default*
FNSP_nisFlatContext_default::from_address(
    const FN_ref_addr &from_addr, const FN_ref& from_ref, FN_status& stat)
{
	FNSP_nisFlatContext_default *answer = new
	    FNSP_nisFlatContext_default(from_addr, from_ref);

	if (answer && answer->my_reference && answer->my_address)
		stat.set_success();
	else {
		if (answer) {
			delete answer;
			answer = 0;
		}
		stat.set(FN_E_INSUFFICIENT_RESOURCES);
	}
	return (answer);
}


// Construct references for names bound in the flat
// contexts we use for files/nis, namely org// and the only name
// bound is "service"/"_service" and under service, "printer"

FN_ref*
FNSP_nisFlatContext_default::resolve(const FN_string &name,
    FN_status_csvc& cstat)
{
	FN_ref* ref = 0;

	switch (my_address->get_context_type()) {
	case FNSP_nsid_context:
		if (name.compare(FNSP_Service_Name,
		    FN_STRING_CASE_INSENSITIVE) == 0 ||
		    name.compare(FNSP_Service_Name2,
		    FN_STRING_CASE_INSENSITIVE) == 0)
			ref = make_service_ref();
		break;
	case FNSP_service_context:
		if (name.compare(FNSP_Printer_Name,
				    FN_STRING_CASE_INSENSITIVE) == 0)
			ref = make_printername_ref();
		break;
	}
	if (ref == 0)
		cstat.set_error(FN_E_NAME_NOT_FOUND, *my_reference, name);
	else
		cstat.set_success();
	return (ref);
}

FN_ref*
FNSP_nisFlatContext_default::c_lookup(const FN_string &name, unsigned int,
    FN_status_csvc &cstat)
{
	FN_ref *answer = 0;
	if (name.is_empty()) {
		// Return reference of current context
		answer = new FN_ref(*my_reference);
		if (answer)
			cstat.set_success();
		else
			cstat.set(FN_E_INSUFFICIENT_RESOURCES);
	} else
		answer = resolve(name, cstat);

	return (answer);
}

// When the given name is null, return names bound in current context.
// When the given name is not null, resolve the name and if successful,
// set reference to be continued

FN_namelist*
FNSP_nisFlatContext_default::c_list_names(const FN_string &name,
    FN_status_csvc& cstat)
{
	if (name.is_empty()) {
		// listing all names bound in current context
		FN_nameset *ns = new FN_nameset;
		if (ns == 0) {
			cstat.set(FN_E_INSUFFICIENT_RESOURCES);
			return (0);
		}
		switch (my_address->get_context_type()) {
		case FNSP_nsid_context:
			ns->add(FNSP_Service_Name);
			ns->add(FNSP_Service_Name2);
			break;
		case FNSP_service_context:
			ns->add(FNSP_Printer_Name);
			break;
		}
		cstat.set_success();
		return (new FN_namelist_svc(ns));
	} else {
		// resolve name and have list be performed there
		FN_ref *ref = resolve(name, cstat);

		if (cstat.is_success()) {
			cstat.set(FN_E_SPI_CONTINUE, ref, 0, &empty_name);
			delete ref;
		}
	}
	return (0);
}

// When the given name is null, return bindings bound in current context.
// When the given name is not null, resolve the name and if successful,
// return reference to be continued.

FN_bindinglist *
FNSP_nisFlatContext_default::c_list_bindings(const FN_string &name,
    FN_status_csvc& cstat)
{
	if (!name.is_empty()) {
		FN_ref *ref = resolve(name, cstat);
		if (cstat.is_success()) {
			cstat.set(FN_E_SPI_CONTINUE, ref, 0, &empty_name);
			delete ref;
		}
		return (0);
	}

	FN_ref *ref;
	FN_bindingset *bs = new FN_bindingset;
	if (bs == 0) {
		cstat.set(FN_E_INSUFFICIENT_RESOURCES);
		return (0);
	}

	switch (my_address->get_context_type()) {
	case FNSP_nsid_context:
		ref = make_service_ref();
		bs->add(FNSP_Service_Name, *ref);
		bs->add(FNSP_Service_Name2, *ref);
		delete ref;
		break;
	case FNSP_service_context:
		ref = make_printername_ref();
		bs->add(FNSP_Printer_Name, *ref);
		delete ref;
		break;
	}

	cstat.set_success();
	return (new FN_bindinglist_svc(bs));
}

FN_attrset *
FNSP_nisFlatContext_default::c_get_syntax_attrs(const FN_string &name,
    FN_status_csvc &cstat)
{
	FN_attrset * answer = 0;
	if (name.is_empty()) {
		answer = FNSP_Syntax(FNSP_nsid_context)->get_syntax_attrs();
		cstat.set_success();
	} else {
		FN_ref* ref = resolve(name, cstat);
		if (cstat.is_success()) {
			// Names in a flat context are null context refs
			answer = FNSP_Syntax(FNSP_service_context)->
			    get_syntax_attrs();
			delete ref;
		}
	}
	if (answer == 0 && cstat.is_success())
		cstat.set(FN_E_INSUFFICIENT_RESOURCES);

	return (answer);
}

// Return binding associated with nns pointer of given name.
// If given name is null, return NOT FOUND
// (Flat Context implements junction and cannot have an nns).
FN_ref*
FNSP_nisFlatContext_default::c_lookup_nns(const FN_string &name,
	unsigned int lookup_flags, FN_status_csvc &cstat)
{
	if (name.is_empty()) {
		cstat.set_error(FN_E_NAME_NOT_FOUND, *my_reference, name);
		return (0);
	} else
		return (c_lookup(name, lookup_flags, cstat));
}

// If given name is null, return names bound under nns of current context.
// If given name is not null, return names bound under nns of given name.
// In both cases, resolve to desired context then return FN_E_SPI_CONTINUE.

FN_namelist*
FNSP_nisFlatContext_default::c_list_names_nns(const FN_string &name,
    FN_status_csvc& cstat)
{
	FN_ref* ref = c_lookup_nns(name, 0, cstat);

	if (cstat.is_success()) {
		cstat.set(FN_E_SPI_CONTINUE, ref, 0, 0);
		delete ref;
	}
	return (0);
}

// If given name is null, return names bound under nns of current context.
// If given name is not null, return names bound under nns of given name.
// In both cases, resolve to desired context then return FN_E_SPI_CONTINUE.
// In other words, do exactly the same thing as c_list_names_nns.
FN_bindinglist *
FNSP_nisFlatContext_default::c_list_bindings_nns(const FN_string &name,
    FN_status_csvc &cstat)
{
	FN_ref* ref = c_lookup_nns(name, 0, cstat);

	if (cstat.is_success()) {
		cstat.set(FN_E_SPI_CONTINUE, ref, 0);
		delete ref;
	}
	return (0);
}

// Used by FNSP_nisAddress
// set 'tab' to entire 'name'.
int
FNSP_decompose_index_name(const FN_string &name, FN_string &tab, FN_string &)
{
	tab = name;
	return (0);
}

unsigned int
FNSP_set_access_flags(const FN_string &, unsigned int &flags)
{
	return (flags);
}

static const FN_identifier
FNSP_default_nis_address_type((unsigned char *) "onc_fn_nis");

FN_ref *
FNSP_nisFlatContext_default::make_service_ref()
{
	return (FNSP_reference(FNSP_default_nis_address_type,
	    my_address->get_internal_name(), FNSP_service_context));
}

static const FN_identifier
FNSP_default_printer_nis_address_type((unsigned char *) "onc_fn_printer_nis");

FN_ref *
FNSP_nisFlatContext_default::make_printername_ref()
{
	return (FNSP_reference(FNSP_default_printer_nis_address_type,
	    *FNSP_reftype_from_ctxtype(FNSP_printername_context),
	    my_address->get_internal_name(),
	    FNSP_printername_context,
	    FNSP_normal_repr));
}
