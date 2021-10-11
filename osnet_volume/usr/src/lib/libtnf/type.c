/*
 *	Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#pragma	ident	"@(#)type.c	1.12	94/10/04 SMI"

#include "libtnf.h"

/*
 * All types are struct records
 */

void
_tnf_check_type(tnf_datum_t datum)
{
	CHECK_RECORD(datum);
	CHECK_SLOTS(datum);

	if (!INFO_TYPE(DATUM_INFO(datum)))
		_tnf_error(DATUM_TNF(datum), TNF_ERR_TYPEMISMATCH);
}

/*
 * Get data kind of a type record
 */

tnf_kind_t
tnf_type_get_kind(tnf_datum_t datum)
{
	struct taginfo	*info;

	CHECK_TYPE(datum);

	/* Note: DATUM_RECORD(), not DATUM_TAG() */
	/* LINTED pointer cast may result in improper alignment */
	info = _tnf_get_info(DATUM_TNF(datum), DATUM_RECORD(datum));
	return (info->kind);
}

/*
 * Retrieve type name for datum type record
 */

char *
tnf_type_get_name(tnf_datum_t datum)
{
	CHECK_TYPE(datum);
	/* XXX Dispatch to ABI routine; faster than taginfo lookup? */
	/* LINTED pointer cast may result in improper alignment */
	return (_tnf_get_name(DATUM_TNF(datum), DATUM_RECORD(datum)));
}

/*
 * Fetch size member of info for datum type record
 */

size_t
tnf_type_get_size(tnf_datum_t datum)
{
	struct taginfo	*info;

	CHECK_TYPE(datum);

	/* Note: DATUM_RECORD(), not DATUM_TAG() */
	/* LINTED pointer cast may result in improper alignment */
	info = _tnf_get_info(DATUM_TNF(datum), DATUM_RECORD(datum));

	if (INFO_ARRAY(info))
		/* XXX All arrays are self-sized */
		return ((size_t)-1);
	else
		return (info->size);
}

/*
 * Get the base type of a type
 */

tnf_datum_t
tnf_type_get_base(tnf_datum_t datum)
{
	struct taginfo	*info;

	CHECK_TYPE(datum);

	/* Note: DATUM_RECORD(), not DATUM_TAG() */
	/* LINTED pointer cast may result in improper alignment */
	info = _tnf_get_info(DATUM_TNF(datum), DATUM_RECORD(datum));

	if (INFO_DERIVED(info))
		return (DATUM(info->base->meta, (caddr_t)info->base->tag));
	else
		return (datum);
}

/*
 * If type record has named property, return a datum for it
 */

tnf_datum_t
tnf_type_get_property(tnf_datum_t datum, char *name)
{
	tnf_ref32_t	*property;

	CHECK_TYPE(datum);

	/* Note: DATUM_RECORD(), not DATUM_TAG() */
	property = _tnf_get_property(DATUM_TNF(datum),
		/* LINTED pointer cast may result in improper alignment */
			DATUM_RECORD(datum), name);

	if (property == TNF_NULL)
		return (TNF_DATUM_NULL);
	else
		return (RECORD_DATUM(DATUM_TNF(datum), property));
}
