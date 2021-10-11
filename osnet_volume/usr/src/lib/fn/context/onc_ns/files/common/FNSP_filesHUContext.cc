/*
 * Copyright (c) 1992 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_filesHUContext.cc	1.1	96/03/31 SMI"

#include "FNSP_filesHUContext.hh"

static const FN_composite_name empty_name((const unsigned char *) "");

FNSP_filesHUContext::~FNSP_filesHUContext()
{
	// handled by superclass
}

FNSP_filesHUContext::FNSP_filesHUContext(
    const FN_string &dirname, unsigned context_type) :
FNSP_filesFlatContext(dirname, context_type)
{
}

FNSP_filesHUContext::FNSP_filesHUContext(const FN_ref &from_ref) :
FNSP_filesFlatContext(from_ref)
{
}

FNSP_filesHUContext::FNSP_filesHUContext(
    const FN_ref_addr &from_addr, const FN_ref &from_ref) :
FNSP_filesFlatContext(from_addr, from_ref)
{
}

FNSP_filesHUContext*
FNSP_filesHUContext::from_address(const FN_ref_addr &from_addr,
    const FN_ref &from_ref,
    FN_status &stat)
{
	FNSP_filesHUContext *answer = new FNSP_filesHUContext(from_addr,
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
FNSP_filesHUContext::c_list_names(const FN_string &name, FN_status_csvc &cstat)
{
	if (name.is_empty()) {
		unsigned status;
		FN_namelist *answer =
			files_impl->list_names_hu(status);
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
FNSP_filesHUContext::c_list_bindings(const FN_string &name,
    FN_status_csvc &cstat)
{
	if (name.is_empty()) {
		unsigned status;
		FN_bindinglist *answer =
			files_impl->list_bindings_hu(status);
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
