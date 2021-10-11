/*
 *	Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#pragma	ident	"@(#)record.c	1.8	94/10/04 SMI"

#include "libtnf.h"

/*
 * Check a record datum
 */

void
_tnf_check_record(tnf_datum_t datum)
{
	CHECK_DATUM(datum);

	/* All records must be tagged */
	if (!INFO_TAGGED(DATUM_INFO(datum)))
		_tnf_error(DATUM_TNF(datum), TNF_ERR_TYPEMISMATCH);
}

/*
 * Retrieve the tag arg, encoded in low 16 bits of tag word
 */

tnf_datum_t
tnf_get_tag_arg(tnf_datum_t datum)
{
	TNF		*tnf;
	tnf_ref32_t	*arg;

	CHECK_RECORD(datum);

	tnf = DATUM_TNF(datum);

	/* Should not give an error if not found */
	/* LINTED pointer cast may result in improper alignment */
	arg = _tnf_get_tag_arg(tnf, DATUM_RECORD(datum));

	if (arg == TNF_NULL)
		return (TNF_DATUM_NULL);
	else			/* repackage the tag arg with its taginfo */
		return (RECORD_DATUM(tnf, arg));
}
