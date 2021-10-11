/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_nisHUContext.cc	1.1	96/03/31 SMI"

#include "FNSP_nisHUContext.hh"
#include "FNSP_nisAddress.hh"

static const FN_composite_name empty_name((const unsigned char *) "");

//  A flat naming system is one in which only non-hierarchical names are bound.
//  All names in the naming system are bound in a single context.
//  These names are junctions.
//
//  The bindings of the junctions
//  are stored in the binding table of the flat context.
//  The reference bound to a flat name is the same as its nns.
//

FNSP_nisHUContext::~FNSP_nisHUContext()
{
	// handled by superclass
}

FNSP_nisHUContext::FNSP_nisHUContext(
    const FN_string &dirname, unsigned context_type) :
FNSP_nisFlatContext(dirname, context_type)
{
}

FNSP_nisHUContext::FNSP_nisHUContext(const FN_ref &from_ref) :
FNSP_nisFlatContext(from_ref)
{
}

FNSP_nisHUContext::FNSP_nisHUContext(
    const FN_ref_addr &from_addr, const FN_ref &from_ref) :
FNSP_nisFlatContext(from_addr, from_ref)
{
}

FNSP_nisHUContext*
FNSP_nisHUContext::from_address(const FN_ref_addr &from_addr,
    const FN_ref &from_ref,
    FN_status &stat)
{
	FNSP_nisHUContext *answer = new FNSP_nisHUContext(from_addr,
	    from_ref);

	if (answer && answer->my_reference != NULL &&
	    answer->ns_impl != NULL && answer->ns_impl->my_address != NULL)
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

FN_namelist*
FNSP_nisHUContext::c_list_names(const FN_string &name, FN_status_csvc &cstat)
{
	if (name.is_empty()) {
		unsigned status;
		FN_namelist *answer = nis_impl->list_names_hu(status);
		if (status == FN_SUCCESS && answer) {
			cstat.set_success();
			return (answer);
		} else
			cstat.set_error(status, *my_reference, name);
	} else {
		// resolve name and have list be performed there
		FN_ref *ref = resolve(name, 0, cstat);

		if (cstat.is_success()) {
			cstat.set(FN_E_SPI_CONTINUE, ref, 0, &empty_name);
			delete ref;
		}
	}
	return (0);
}

FN_bindinglist*
FNSP_nisHUContext::c_list_bindings(const FN_string &name, FN_status_csvc &cstat)
{
	if (name.is_empty()) {
		unsigned status;
		FN_bindinglist *answer = nis_impl->list_bindings_hu(status);
		if (status == FN_SUCCESS && answer) {
			cstat.set_success();
			return (answer);
		} else
			cstat.set_error(status, *my_reference, name);
	} else {
		// resolve name and have list be performed there
		FN_ref *ref = resolve(name, 0, cstat);

		if (cstat.is_success()) {
			cstat.set(FN_E_SPI_CONTINUE, ref, 0, &empty_name);
			delete ref;
		}
	}
	return (0);
}
