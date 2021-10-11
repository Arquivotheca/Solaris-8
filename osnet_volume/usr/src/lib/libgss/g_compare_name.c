/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)g_compare_name.c	1.13	98/01/22 SMI"

/*
 *  glue routine for gss_compare_name
 *
 */

#include <mechglueP.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>

OM_uint32
gss_compare_name(minor_status,
			name1,
			name2,
			name_equal)

OM_uint32 *		minor_status;
const gss_name_t	name1;
const gss_name_t	name2;
int *			name_equal;

{
	OM_uint32		major_status, temp_minor;
	gss_union_name_t	union_name1, union_name2;
	gss_mechanism		mech;
	gss_name_t		internal_name;

	if (minor_status == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);
	*minor_status = 0;

	if (name1 == 0 || name2 == 0)
		return (GSS_S_CALL_INACCESSIBLE_READ | GSS_S_BAD_NAME);

	if (name_equal == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);

	union_name1 = (gss_union_name_t) name1;
	union_name2 = (gss_union_name_t) name2;
	/*
	 * Try our hardest to make union_name1 be the mechanism-specific
	 * name.  (Of course we can't if both names aren't
	 * mechanism-specific.)
	 */
	if (union_name1->mech_type == 0) {
		union_name1 = (gss_union_name_t) name2;
		union_name2 = (gss_union_name_t) name1;
	}
	/*
	 * If union_name1 is mechanism specific, then fetch its mechanism
	 * information.
	 */
	if (union_name1->mech_type) {
		mech = __gss_get_mechanism(union_name1->mech_type);
		if (!mech)
			return (GSS_S_BAD_MECH);
		if (!mech->gss_compare_name)
			return (GSS_S_UNAVAILABLE);
	}

	*name_equal = 0;	/* Default to *not* equal.... */

	/*
	 * First case... both names are mechanism-specific
	 */
	if (union_name1->mech_type && union_name2->mech_type) {
		if (!g_OID_equal(union_name1->mech_type,
					union_name2->mech_type))
			return (GSS_S_COMPLETE);
		if ((union_name1->mech_name == 0) ||
			(union_name2->mech_name == 0))
			/* should never happen */
			return (GSS_S_BAD_NAME);
		return (mech->gss_compare_name(mech->context, minor_status,
							union_name1->mech_name,
							union_name2->mech_name,
							name_equal));
	}

	/*
	 * Second case... both names are NOT mechanism specific.
	 *
	 * All we do here is make sure the two name_types are equal and then
	 * that the external_names are equal. Note the we do not take care
	 * of the case where two different external names map to the same
	 * internal name. We cannot determine this, since we as yet do not
	 * know what mechanism to use for calling the underlying
	 * gss_import_name().
	 */
	if (!union_name1->mech_type && !union_name2->mech_type) {
		if (!g_OID_equal(union_name1->name_type,
					union_name2->name_type))
			return (GSS_S_COMPLETE);
		if ((union_name1->external_name->length !=
			union_name2->external_name->length) ||
			(memcmp(union_name1->external_name->value,
				union_name2->external_name->value,
				union_name1->external_name->length) != 0))
			return (GSS_S_COMPLETE);
		*name_equal = 1;
		return (GSS_S_COMPLETE);
	}

	/*
	 * Final case... one name is mechanism specific, the other isn't.
	 *
	 * We attempt to convert the general name to the mechanism type of
	 * the mechanism-specific name, and then do the compare.  If we
	 * can't import the general name, then we return that the name is
	 * _NOT_ equal.
	 */
	if (union_name2->mech_type) {
		/* We make union_name1 the mechanism specific name. */
		union_name1 = (gss_union_name_t) name2;
		union_name2 = (gss_union_name_t) name1;
	}
	major_status = __gss_import_internal_name(minor_status,
							union_name1->mech_type,
							union_name2,
							&internal_name);
	if (major_status != GSS_S_COMPLETE)
		return (GSS_S_COMPLETE); /* return complete, but not equal */

	major_status = mech->gss_compare_name(mech->context, minor_status,
							union_name1->mech_name,
							internal_name,
							name_equal);
	__gss_release_internal_name(&temp_minor, union_name1->mech_type,
					&internal_name);
	return (major_status);
}
