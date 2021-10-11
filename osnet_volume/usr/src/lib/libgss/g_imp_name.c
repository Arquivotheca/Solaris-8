/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)g_imp_name.c	1.21	98/07/20 SMI"

/*
 *  glue routine gss_import_name
 *
 */

#include <mechglueP.h>
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>
#include <errno.h>

extern int
get_der_length(unsigned char **, unsigned int, unsigned int *);

/* local function to import GSS_C_EXPORT_NAME names */
static OM_uint32 importExportName(OM_uint32 *, gss_union_name_t);


OM_uint32
gss_import_name(minor_status,
		input_name_buffer,
		input_name_type,
		output_name)

OM_uint32 *minor_status;
const gss_buffer_t input_name_buffer;
const gss_OID input_name_type;
gss_name_t *output_name;
{
	gss_union_name_t union_name;
	OM_uint32 major_status = GSS_S_FAILURE, tmp;

	/* check output parameters */
	if (!minor_status)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);

	*minor_status = 0;

	if (GSS_EMPTY_BUFFER(input_name_buffer) || input_name_type == NULL)
		return (GSS_S_CALL_INACCESSIBLE_READ);

	if (output_name == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);

	*output_name = 0;

	/*
	 * First create the union name struct that will hold the external
	 * name and the name type.
	 */
	union_name = (gss_union_name_t) malloc(sizeof (gss_union_name_desc));
	if (!union_name)
		return (GSS_S_FAILURE);

	union_name->mech_type = 0;
	union_name->mech_name = 0;
	union_name->name_type = 0;
	union_name->external_name = 0;

	/*
	 * All we do here is record the external name and name_type.
	 * When the name is actually used, the underlying gss_import_name()
	 * is called for the appropriate mechanism.  The exception to this
	 * rule is when the name of GSS_C_NT_EXPORT_NAME type.  If that is
	 * the case, then we make it MN in this call.
	 */
	major_status = __gss_create_copy_buffer(input_name_buffer,
					&union_name->external_name, 0);
	if (major_status != GSS_S_COMPLETE) {
		free(union_name);
		return (major_status);
	}

	major_status = generic_gss_copy_oid(minor_status, input_name_type,
						&union_name->name_type);
	if (major_status != GSS_S_COMPLETE)
		goto allocation_failure;

	/*
	 * In MIT Distribution the mechanism is determined from the nametype;
	 * This is not a good idea - first mechanism that supports a given
	 * name type is picked up; later on the caller can request a
	 * different mechanism. So we don't determine the mechanism here. Now
	 * the user level and kernel level import_name routine looks similar
	 * except the kernel routine makes a copy of the nametype structure. We
	 * do however make this an MN for names of GSS_C_NT_EXPORT_NAME type.
	 */
	if (g_OID_equal(input_name_type, GSS_C_NT_EXPORT_NAME)) {
		major_status = importExportName(minor_status, union_name);
		if (major_status != GSS_S_COMPLETE)
			goto allocation_failure;
	}

	*output_name = (gss_name_t) union_name;
	return (GSS_S_COMPLETE);

allocation_failure:
	if (union_name) {
		if (union_name->external_name) {
			if (union_name->external_name->value)
				free(union_name->external_name->value);
			free(union_name->external_name);
		}
		if (union_name->name_type)
			generic_gss_release_oid(&tmp, &union_name->name_type);
		if (union_name->mech_name)
			__gss_release_internal_name(minor_status,
						union_name->mech_type,
						&union_name->mech_name);
		if (union_name->mech_type)
			generic_gss_release_oid(&tmp, &union_name->mech_type);
		free(union_name);
	}
	return (major_status);
}


/*
 * GSS export name constants
 */
static const char *expNameTokId = "\x04\x01";
static const int expNameTokIdLen = 2;
static const int mechOidLenLen = 2;

static OM_uint32
importExportName(minor, unionName)
OM_uint32 *minor;
gss_union_name_t unionName;
{
	gss_OID_desc mechOid, nameType;
	gss_buffer_desc expName;
	unsigned char *buf;
	gss_mechanism mech;
	OM_uint32 major, nameLen, curLength;
	unsigned int bytes;

	expName.value = unionName->external_name->value;
	expName.length = unionName->external_name->length;

	curLength = expNameTokIdLen + mechOidLenLen;
	if (expName.length < curLength)
		return (GSS_S_DEFECTIVE_TOKEN);

	buf = (unsigned char *)expName.value;
	if (memcmp(expNameTokId, buf, expNameTokIdLen) != 0)
		return (GSS_S_DEFECTIVE_TOKEN);

	buf += expNameTokIdLen;

	/* extract the mechanism oid length */
	mechOid.length = (*buf++ << 8);
	mechOid.length |= (*buf++);
	curLength += mechOid.length;
	if (expName.length < curLength)
		return (GSS_S_DEFECTIVE_TOKEN);
	/*
	 * The mechOid itself is encoded in DER format, OID Tag (0x06)
	 * length and the value of mech_OID
	*/
	if (*buf++ != 0x06)
		return (GSS_S_DEFECTIVE_TOKEN);

	/*
	 * mechoid Length is encoded twice; once in 2 bytes as
	 * explained in RFC 1510 (under mechanism independent exported
	 * name object format) and once using DER encoding
	 * We use the DER encoded length since it can accomadate
	 * arbitrariliy large values of OID.
	 */

	mechOid.length = get_der_length(&buf,
				(expName.length - curLength), &bytes);
	mechOid.elements = (void *)buf;
	buf += mechOid.length;
	if ((mech = __gss_get_mechanism(&mechOid)) == NULL)
		return (GSS_S_BAD_MECH);

	if (mech->gss_import_name == NULL)
		return (GSS_S_UNAVAILABLE);

	/*
	 * we must now determine if we should unwrap the name ourselves
	 * or make the mechanism do it - we should only unwrap it
	 * if we create it; so if mech->gss_export_name == NULL, we must
	 * have created it.
	 */
	if (mech->gss_export_name) {
		if ((major = mech->gss_import_name(mech->context, minor,
				&expName, (gss_OID)GSS_C_NT_EXPORT_NAME,
				&unionName->mech_name)) != GSS_S_COMPLETE ||
			(major = generic_gss_copy_oid(minor, &mechOid,
					&unionName->mech_type)) !=
				GSS_S_COMPLETE) {
			return (major);
		}
		return (major);
	}
	/*
	 * we must have exported the name - so we now need to reconstruct it
	 * and call the mechanism to create it
	 */
	curLength += 4;		/* 4 bytes for name len */
	if (expName.length < (curLength + 2))	/* +2 is for nameType length */
		return (GSS_S_DEFECTIVE_TOKEN);

	/* next 4 bytes in the name are the name length */
	nameLen = (*buf++) << 24;
	nameLen |= (*buf++ << 16);
	nameLen |= (*buf++ << 8);
	nameLen |= (*buf++);

	/* next two bytes are the name oid */
	nameType.length = (*buf++) << 8;
	nameType.length |= (*buf++);

	curLength += nameLen;	/* this is the total length */
	if (expName.length < curLength)
		return (GSS_S_DEFECTIVE_TOKEN);

	nameType.elements = (void *)buf;
	expName.length = nameLen - 2 - nameType.length;
	expName.value = (void *)(buf + nameType.length);
	major = mech->gss_import_name(mech->context, minor, &expName,
				&nameType, &unionName->mech_name);
	if (major != GSS_S_COMPLETE)
		return (major);

	return (generic_gss_copy_oid(minor, &mechOid, &unionName->mech_type));
} /* importExportName */
