/*
 *	Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#pragma	ident	"@(#)datum.c	1.14	94/10/04 SMI"

#include "libtnf.h"

/*
 * Defines
 */

#define	DATUM_KIND(d)	(DATUM_INFO(d)->kind)

/*
 * Declarations
 */

static int 	has_prop(tnf_datum_t, tag_props_t);

/*
 * Datum operations: for more debuggability
 */

#ifndef _DATUM_MACROS

tnf_datum_t
_tnf_datum(struct taginfo *info, caddr_t val)
{
	return (_DATUM(info, val));
}

struct taginfo *
_tnf_datum_info(tnf_datum_t datum)
{
	return ((struct taginfo *)_DATUM_HI(datum));
}

caddr_t
_tnf_datum_val(tnf_datum_t datum)
{
	return ((caddr_t)_DATUM_LO(datum));
}

#endif

/*
 * Check for valid datum
 */

void
_tnf_check_datum(tnf_datum_t datum)
{
	caddr_t		val;
	TNF		*tnf;

	if (datum == TNF_DATUM_NULL)
		_tnf_error(NULL, TNF_ERR_BADTNF);

	val 	= DATUM_VAL(datum);
	tnf	= DATUM_TNF(datum);

	if ((val <= tnf->file_start) || (val >= tnf->file_end))
		_tnf_error(tnf, TNF_ERR_BADDATUM);
}

/*
 * Retrieve datum kind from cached information
 */

tnf_kind_t
tnf_get_kind(tnf_datum_t datum)
{
	CHECK_DATUM(datum);
	/* The kind field is always completely initialized */
	return (DATUM_KIND(datum));
}

/*
 * Classification predicates: check the cached tag props
 */

static int
has_prop(tnf_datum_t datum, tag_props_t prop)
{
	CHECK_DATUM(datum);

	/* Note: No need to get base info because props inherited */
	return (INFO_PROP(DATUM_INFO(datum), prop));
}

int
tnf_is_inline(tnf_datum_t datum)
{
	return (has_prop(datum, TAG_PROP_INLINE));
}

int
tnf_is_scalar(tnf_datum_t datum)
{
	return (has_prop(datum, TAG_PROP_SCALAR));
}

int
tnf_is_record(tnf_datum_t datum) /* XXX was: tnf_is_tagged */
{
	return (has_prop(datum, TAG_PROP_TAGGED));
}

int
tnf_is_array(tnf_datum_t datum)
{
	return (has_prop(datum, TAG_PROP_ARRAY));
}

int
tnf_is_string(tnf_datum_t datum)
{
	return (has_prop(datum, TAG_PROP_STRING));
}

int
tnf_is_struct(tnf_datum_t datum)
{
	return (has_prop(datum, TAG_PROP_STRUCT));
}

int
tnf_is_type(tnf_datum_t datum)
{
	return (has_prop(datum, TAG_PROP_TYPE));
}

/*
 * Get the type datum for any datum
 */

tnf_datum_t
tnf_get_type(tnf_datum_t datum)
{
	struct taginfo	*info;

	CHECK_DATUM(datum);

	info = DATUM_INFO(datum);
	return (DATUM(info->meta, (caddr_t)info->tag));
}

/*
 * Get the type name for any datum
 * XXX Beware: this is a pointer into the file
 */

char *
tnf_get_type_name(tnf_datum_t datum)
{
	CHECK_DATUM(datum);
	return (DATUM_INFO(datum)->name);	/* cached */
}

/*
 * Get the size of any datum
 */

size_t
tnf_get_size(tnf_datum_t datum)
{
	struct taginfo	*info;
	size_t		size;

	CHECK_DATUM(datum);

	info = DATUM_INFO(datum);
	size = info->size;

	if (size == (size_t)-1)	/* self sized */
		/* XXX tnf_get_slot_named(datum, TNF_N_SELF_SIZE) */
		/* LINTED pointer cast may result in improper alignment */
		return (_tnf_get_self_size(info->tnf, DATUM_RECORD(datum)));
	else
		return (size);
}

/*
 * Get raw pointer to any datum
 */

caddr_t
tnf_get_raw(tnf_datum_t datum)
{
	CHECK_DATUM(datum);
	return (DATUM_VAL(datum));
}
