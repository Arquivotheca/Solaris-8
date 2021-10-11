/*
 * Copyright (c) 1995-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)FNSP_defaultContext.cc	1.1	96/03/31 SMI"

#include "FNSP_defaultContext.hh"

FNSP_defaultContext::~FNSP_defaultContext()
{
}

FN_ref*
FNSP_defaultContext::get_ref(FN_status &stat) const
{
	FN_ref* answer = new FN_ref(*my_reference);

	if (answer)
		stat.set_success();
	else
		stat.set(FN_E_INSUFFICIENT_RESOURCES);

	return (answer);
}

FN_composite_name *
FNSP_defaultContext::equivalent_name(const FN_composite_name &name,
    const FN_string &, FN_status &status)
{
	status.set(FN_E_OPERATION_NOT_SUPPORTED, my_reference, &name);
	return (0);
}

// For FNSP flat contexts, we do not allow explicit bindings to names;
// Bindings to names are generated algorithmically.

int
FNSP_defaultContext::c_bind(const FN_string &name, const FN_ref&,
    unsigned, FN_status_csvc &cstat)
{
	cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

int
FNSP_defaultContext::c_unbind(const FN_string &name, FN_status_csvc& cstat)
{
	cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

int
FNSP_defaultContext::c_rename(const FN_string &name, const FN_composite_name&,
    unsigned, FN_status_csvc &cstat)
{
	cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

// Flat context has no subcontexts
FN_ref*
FNSP_defaultContext::c_create_subcontext(const FN_string &name,
    FN_status_csvc &cstat)
{
	cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

int
FNSP_defaultContext::c_destroy_subcontext(const FN_string &name,
    FN_status_csvc &cstat)
{
	cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_attribute*
FNSP_defaultContext::c_attr_get(const FN_string &name,
    const FN_identifier&, unsigned int, FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

int
FNSP_defaultContext::c_attr_modify(const FN_string &name,
	unsigned int, const FN_attribute&, unsigned int, FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_valuelist*
FNSP_defaultContext::c_attr_get_values(const FN_string &name,
	const FN_identifier&, unsigned int, FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_attrset*
FNSP_defaultContext::c_attr_get_ids(const FN_string &name, unsigned int,
    FN_status_csvc& cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_multigetlist*
FNSP_defaultContext::c_attr_multi_get(const FN_string &name,
    const FN_attrset *, unsigned int, FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

int
FNSP_defaultContext::c_attr_multi_modify(const FN_string &name,
    const FN_attrmodlist&, unsigned int, FN_attrmodlist**,
    FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

int
FNSP_defaultContext::c_bind_nns(const FN_string &name, const FN_ref&,
    unsigned, FN_status_csvc &cstat)
{
	cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

int
FNSP_defaultContext::c_unbind_nns(const FN_string &name, FN_status_csvc& cstat)
{
	cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

int
FNSP_defaultContext::c_rename_nns(const FN_string &name,
    const FN_composite_name&, unsigned, FN_status_csvc &cstat)
{
	cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_ref*
FNSP_defaultContext::c_create_subcontext_nns(const FN_string &name,
    FN_status_csvc &cstat)
{
	cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

int
FNSP_defaultContext::c_destroy_subcontext_nns(const FN_string &name,
    FN_status_csvc &cstat)
{
	cstat.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_attrset *
FNSP_defaultContext::c_get_syntax_attrs_nns(const FN_string &name,
    FN_status_csvc &cstat)
{
	FN_ref* ref = c_lookup_nns(name, 0, cstat);
	if (cstat.is_success())
		cstat.set(FN_E_SPI_CONTINUE, ref, 0);

	if (ref)
		delete ref;
	return (0);
}

FN_attribute*
FNSP_defaultContext::c_attr_get_nns(const FN_string &name,
    const FN_identifier&, unsigned int, FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

int
FNSP_defaultContext::c_attr_modify_nns(const FN_string &name,
    unsigned int, const FN_attribute&, unsigned int, FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_valuelist*
FNSP_defaultContext::c_attr_get_values_nns(const FN_string &name,
    const FN_identifier&, unsigned int, FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_attrset*
FNSP_defaultContext::c_attr_get_ids_nns(const FN_string &name, unsigned int,
    FN_status_csvc& cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_multigetlist*
FNSP_defaultContext::c_attr_multi_get_nns(const FN_string &name,
    const FN_attrset *, unsigned int, FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

int
FNSP_defaultContext::c_attr_multi_modify_nns(const FN_string &name,
    const FN_attrmodlist&, unsigned int, FN_attrmodlist**,
    FN_status_csvc &cs)
{
	cs.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

// Extended attribute operations
int
FNSP_defaultContext::c_attr_bind(const FN_string &name,
    const FN_ref & /* ref */, const FN_attrset * /* attrs */,
    unsigned int /* exclusive */, FN_status_csvc &status)
{
	status.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_ref *
FNSP_defaultContext::c_attr_create_subcontext(const FN_string &name,
    const FN_attrset * /* attr */, FN_status_csvc &status)
{
	status.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_searchlist *
FNSP_defaultContext::c_attr_search(const FN_string &name,
    const FN_attrset * /* match_attrs */, unsigned int /* return_ref */,
    const FN_attrset * /* return_attr_ids */, FN_status_csvc &status)
{
	status.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_ext_searchlist *
FNSP_defaultContext::c_attr_ext_search(const FN_string &name,
    const FN_search_control * /* control */,
    const FN_search_filter * /* filter */,
    FN_status_csvc &status)
{
	status.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

int
FNSP_defaultContext::c_attr_bind_nns(const FN_string &name,
    const FN_ref & /* ref */, const FN_attrset * /* attrs */,
    unsigned int /* exclusive */, FN_status_csvc &status)
{
	status.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_ref *
FNSP_defaultContext::c_attr_create_subcontext_nns(const FN_string &name,
    const FN_attrset * /* attr */, FN_status_csvc &status)
{
	status.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_searchlist *
FNSP_defaultContext::c_attr_search_nns(const FN_string &name,
    const FN_attrset * /* match_attrs */, unsigned int /* return_ref */,
    const FN_attrset * /* return_attr_ids */, FN_status_csvc &status)
{
	status.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}

FN_ext_searchlist *
FNSP_defaultContext::c_attr_ext_search_nns(const FN_string &name,
    const FN_search_control * /* control */,
    const FN_search_filter * /* filter */,
    FN_status_csvc &status)
{
	status.set_error(FN_E_OPERATION_NOT_SUPPORTED, *my_reference, name);
	return (0);
}
