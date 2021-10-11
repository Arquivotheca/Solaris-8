/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)g_glue.c	1.20	98/07/20 SMI"

#include <mechglueP.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>

#define	MSO_BIT (8*(sizeof (int) - 1))  /* Most significant octet bit */

/*
 * This file contains the support routines for the glue layer.
 */

/*
 * get_der_length: Givin a pointer to a buffer that contains a DER encoded
 * length, decode the length updating the buffer to point to the character
 * after the DER encoding. The parameter bytes will point to the number of
 * bytes that made up the DER encoding of the length originally pointed to
 * by the buffer. Note we return -1 on error.
 */
int
get_der_length(unsigned char **buf, unsigned int buf_len, unsigned int *bytes)
{
	/* p points to the beginning of the buffer */
	unsigned char *p = *buf;
	int length, new_length;
	int octets;

	if (buf_len < 1)
		return (-1);

	/* We should have at least one byte */
	*bytes = 1;

	/*
	 * If the High order bit is not set then the length is just the value
	 * of *p.
	 */
	if (*p < 128) {
		*buf = p+1;	/* Advance the buffer */
	return (*p);		/* return the length */
	}

	/*
	 * if the High order bit is set, then the low order bits represent
	 * the number of bytes that contain the DER encoding of the length.
	 */

	octets = *p++ & 0x7f;
	*bytes += octets;

	/* See if the supplied buffer contains enough bytes for the length. */
	if (octets > buf_len - 1)
		return (-1);

	/*
	 * Calculate a multibyte length. The length is encoded as an
	 * unsigned integer base 256.
	 */
	for (length = 0; octets; octets--) {
		new_length = (length << 8) + *p++;
		if (new_length < length)  /* overflow */
			return (-1);
		length = new_length;
	}

	*buf = p; /* Advance the buffer */

	return (length);
}

/*
 * der_length_size: Return the number of bytes to encode a given length.
 */
unsigned int
der_length_size(unsigned int len)
{
	int i;

	if (len < 128)
		return (1);

	for (i = 0; len; i++) {
		len >>= 8;
	}

	return (i+1);
}

/*
 * put_der_length: Encode the supplied length into the buffer pointed to
 * by buf. max_length represents the maximum length of the buffer pointed
 * to by buff. We will advance buf to point to the character after the newly
 * DER encoded length. We return 0 on success or -l it the length cannot
 * be encoded in max_len characters.
 */
int
put_der_length(unsigned length, unsigned char **buf, unsigned int max_len)
{
	unsigned char *s = *buf, *p;
	unsigned int buf_len = 0;
	int i;

	/* Oops */
	if (buf == 0 || max_len < 1)
		return (-1);

	/* Single byte is the length */
	if (length < 128) {
		*s++ = length;
		*buf = s;
		return (0);
	}

	/* First byte contains the number of octets */
	p = s + 1;

	/* Running total of the DER encoding length */
	buf_len = 0;

	/*
	 * Encode MSB first. We do the encoding by setting a shift
	 * factor to MSO_BIT (24 for 32 bit words) and then shifting the length
	 * by the factor. We then encode the resulting low order byte.
	 * We subtract 8 from the shift factor and repeat to ecnode the next
	 * byte. We stop when the shift factor is zero or we've run out of
	 * buffer to encode into.
	 */
	for (i = MSO_BIT; i >= 0 && buf_len <= max_len; i -= 8) {
		unsigned int v;
		int first = 0;
		v = (length >> i) & 0xff;
		if ((v) || first) {
			buf_len += 1;
			*p++ = v;
			first = 1;
		}
	}
	if (i >= 0)			/* buffer overflow */
		return (-1);

	/*
	 * We go back now and set the first byte to be the length with
	 * the high order bit set.
	 */
	*s = buf_len | 0x80;
	*buf = p;

	return (0);
}


/*
 *  glue routine for get_mech_type
 *
 */
OM_uint32
__gss_get_mech_type(OID, token)
	gss_OID			OID;
	const gss_buffer_t	token;
{
	unsigned char * buffer_ptr;
	int length;

	/*
	 * This routine reads the prefix of "token" in order to determine
	 * its mechanism type. It assumes the encoding suggested in
	 * Appendix B of RFC 1508. This format starts out as follows :
	 *
	 * tag for APPLICATION 0, Sequence[constructed, definite length]
	 * length of remainder of token
	 * tag of OBJECT IDENTIFIER
	 * length of mechanism OID
	 * encoding of mechanism OID
	 * <the rest of the token>
	 *
	 * Numerically, this looks like :
	 *
	 * 0x60
	 * <length> - could be multiple bytes
	 * 0x06
	 * <length> - assume only one byte, hence OID length < 127
	 * <mech OID bytes>
	 *
	 * The routine fills in the OID value and returns an error as necessary.
	 */

	if (OID == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);

	if ((token == NULL) || (token->value == NULL))
		return (GSS_S_DEFECTIVE_TOKEN);

	/* Skip past the APP/Sequnce byte and the token length */

	buffer_ptr = (unsigned char *) token->value;

	if (*(buffer_ptr++) != 0x60)
		return (GSS_S_DEFECTIVE_TOKEN);
	length = *buffer_ptr++;

	/* check if token length is null */
	if (length == 0)
	    return (GSS_S_DEFECTIVE_TOKEN);

	if (length & 0x80) {
		if ((length & 0x7f) > 4)
			return (GSS_S_DEFECTIVE_TOKEN);
		buffer_ptr += length & 0x7f;
	}

	if (*(buffer_ptr++) != 0x06)
		return (GSS_S_DEFECTIVE_TOKEN);

	OID->length = (OM_uint32) *(buffer_ptr++);
	OID->elements = (void *) buffer_ptr;
	return (GSS_S_COMPLETE);
}


/*
 *  Internal routines to get and release an internal mechanism name
 */
OM_uint32 __gss_import_internal_name(minor_status, mech_type, union_name,
					internal_name)
OM_uint32		*minor_status;
const gss_OID		mech_type;
gss_union_name_t	union_name;
gss_name_t		*internal_name;
{
	OM_uint32			status;
	gss_mechanism		mech;

	mech = __gss_get_mechanism(mech_type);
	if (mech) {
		if (mech->gss_import_name)
			status = mech->gss_import_name(
						mech->context,
						minor_status,
						union_name->external_name,
						union_name->name_type,
						internal_name);
		else
			status = GSS_S_UNAVAILABLE;

		return (status);
	}

	return (GSS_S_BAD_MECH);
}


OM_uint32 __gss_export_internal_name(minor_status, mech_type,
		internal_name, name_buf)
OM_uint32		*minor_status;
const gss_OID		mech_type;
const gss_name_t	internal_name;
gss_buffer_t		name_buf;
{
	OM_uint32 status;
	gss_mechanism mech;
	gss_buffer_desc dispName;
	gss_OID nameOid;
	unsigned char *buf = NULL;
	int nameLen = 0;
	const unsigned char tokId[] = "\x04\x01";
	const int tokIdLen = 2;
	const int mechOidLenLen = 2, mechOidTagLen = 1;
	const nameOidLenLen = 2, nameLenLen = 4;
	int mechOidDERLen = 0;
	int mechOidLen = 0;

	mech = __gss_get_mechanism(mech_type);
	if (!mech)
		return (GSS_S_BAD_MECH);

	if (mech->gss_export_name)
		return (mech->gss_export_name(mech->context,
						minor_status,
						internal_name,
						name_buf));

	/*
	 * if we are here it is because the mechanism does not provide
	 * a gss_export_name so we will use our implementation.  We
	 * do required that the mechanism define a gss_display_name.
	 */
	if (!mech->gss_display_name)
		return (GSS_S_UNAVAILABLE);

	/*
	 * our implementation of gss_export_name puts the
	 * following all in a contiguos buffer:
	 * tok Id + mech oid len + mech oid (in DER format) +
	 * total name len + name oid len + name oid + name.
	 * The name oid and name buffer is obtained by calling
	 * gss_display_name.
	 */
	if ((status = mech->gss_display_name(mech->context,
						minor_status,
						internal_name,
						&dispName,
						&nameOid))
						!= GSS_S_COMPLETE)
		return (status);

	/* determine the size of the buffer needed */
	nameLen = nameOidLenLen +
			nameOid->length + dispName.length;
	mechOidDERLen = der_length_size(mech_type->length);
	name_buf->length = tokIdLen + mechOidLenLen +
				mechOidTagLen + mechOidDERLen+
				mech_type->length +
				nameLenLen + nameLen;
	if ((name_buf->value = (void*)malloc(name_buf->length)) ==
		(void*)NULL) {
			name_buf->length = 0;
			gss_release_buffer(&status, &dispName);
			return (GSS_S_FAILURE);
	}

	/* now create the name ..... */
	buf = (unsigned char *)name_buf->value;
	memset(name_buf->value, '\0', name_buf->length);
	memcpy(buf, tokId, tokIdLen);
	buf += tokIdLen;

	/* spec allows only 2 bytes for the mech oid length */
	mechOidLen = mechOidDERLen + mechOidTagLen + mech_type->length;
	*buf++ = (mechOidLen & 0xFF00) >> 8;
	*buf++ = (mechOidLen & 0x00FF);

	/*
	 * DER Encoding of mech OID contains OID Tag (0x06), length and
	 * mech OID value
	*/
	*buf++ = 0x06;
	if (put_der_length(mech_type->length, &buf,
		(name_buf->length - tokIdLen -2)) != 0) {
		name_buf->length = 0;
		free(name_buf->value);
		gss_release_buffer(&status, &dispName);
		return (GSS_S_FAILURE);
	}

	memcpy(buf, mech_type->elements, mech_type->length);
	buf += mech_type->length;

	/* spec designates the next 4 bytes for the name length */
	*buf++ = (nameLen & 0xFF000000) >> 24;
	*buf++ = (nameLen & 0x00FF0000) >> 16;
	*buf++ = (nameLen & 0x0000FF00) >> 8;
	*buf++ = (nameLen & 0X000000FF);

	/* start our name by putting in the nameOid length  2 bytes only */
	*buf++ = (nameOid->length & 0xFF00) >> 8;
	*buf++ = (nameOid->length & 0x00FF);


	/* now add the nameOid */
	memcpy(buf, nameOid->elements, nameOid->length);
	buf += nameOid->length;

	/* for the final ingredient - add the name from gss_display_name */
	memcpy(buf, dispName.value, dispName.length);

	/* release the buffer obtained from gss_display_name */
	gss_release_buffer(minor_status, &dispName);
	return (GSS_S_COMPLETE);
} /*  __gss_export_internal_name */


OM_uint32 __gss_display_internal_name(minor_status, mech_type, internal_name,
						external_name, name_type)
OM_uint32		*minor_status;
const gss_OID		mech_type;
const gss_name_t	internal_name;
gss_buffer_t		external_name;
gss_OID			*name_type;
{
	OM_uint32			status;
	gss_mechanism		mech;

	mech = __gss_get_mechanism(mech_type);
	if (mech) {
		if (mech->gss_display_name)
			status = mech->gss_display_name(
							mech->context,
							minor_status,
							internal_name,
							external_name,
							name_type);
		else
			status = GSS_S_UNAVAILABLE;

		return (status);
	}

	return (GSS_S_BAD_MECH);
}

OM_uint32
__gss_release_internal_name(minor_status, mech_type, internal_name)
OM_uint32		*minor_status;
const gss_OID		mech_type;
gss_name_t		*internal_name;
{
	OM_uint32			status;
	gss_mechanism		mech;

	mech = __gss_get_mechanism(mech_type);
	if (mech) {
		if (mech->gss_release_name)
			status = mech->gss_release_name(
							mech->context,
							minor_status,
							internal_name);
		else
			status = GSS_S_UNAVAILABLE;

		return (status);
	}

	return (GSS_S_BAD_MECH);
}


/*
 * This function converts an internal gssapi name to a union gssapi
 * name.  Note that internal_name should be considered "consumed" by
 * this call, whether or not we return an error.
 */
OM_uint32 __gss_convert_name_to_union_name(minor_status, mech,
						internal_name, external_name)
	OM_uint32 *minor_status;
	gss_mechanism		mech;
	gss_name_t		internal_name;
	gss_name_t		*external_name;
{
	OM_uint32 major_status, tmp;
	gss_union_name_t union_name;

	union_name = (gss_union_name_t) malloc(sizeof (gss_union_name_desc));
	if (!union_name) {
			goto allocation_failure;
	}
	union_name->mech_type = 0;
	union_name->mech_name = internal_name;
	union_name->name_type = 0;
	union_name->external_name = 0;

	major_status = generic_gss_copy_oid(minor_status, &mech->mech_type,
						&union_name->mech_type);
	if (major_status != GSS_S_COMPLETE)
		goto allocation_failure;

	union_name->external_name =
		(gss_buffer_t) malloc(sizeof (gss_buffer_desc));
	if (!union_name->external_name) {
			goto allocation_failure;
	}

	major_status = mech->gss_display_name(mech->context, minor_status,
						internal_name,
						union_name->external_name,
						&union_name->name_type);
	if (major_status != GSS_S_COMPLETE)
		goto allocation_failure;

	*external_name =  union_name;
	return (GSS_S_COMPLETE);

allocation_failure:
	if (union_name) {
		if (union_name->external_name) {
			if (union_name->external_name->value)
				free(union_name->external_name->value);
			free(union_name->external_name);
		}
		if (union_name->name_type)
			gss_release_oid(&tmp, &union_name->name_type);
		if (union_name->mech_type)
			gss_release_oid(&tmp, &union_name->mech_type);
		free(union_name);
	}
	/*
	 * do as the top comment says - since we are now owners of
	 * internal_name, we must clean it up
	 */
	if (internal_name)
		__gss_release_internal_name(&tmp, &mech->mech_type,
						&internal_name);

	return (major_status);
}

/*
 * Glue routine for returning the mechanism-specific credential from a
 * external union credential.
 */
gss_cred_id_t
__gss_get_mechanism_cred(union_cred, mech_type)
	const gss_union_cred_t	union_cred;
	const gss_OID		mech_type;
{
	int			i;

	if (union_cred == GSS_C_NO_CREDENTIAL)
		return (GSS_C_NO_CREDENTIAL);

	for (i = 0; i < union_cred->count; i++) {
		if (g_OID_equal(mech_type, &union_cred->mechs_array[i]))
			return (union_cred->cred_array[i]);
	}
	return (GSS_C_NO_CREDENTIAL);
}


/*
 * Routine to create and copy the gss_buffer_desc structure.
 * Both space for the structure and the data is allocated.
 */
OM_uint32
__gss_create_copy_buffer(srcBuf, destBuf, addNullChar)
	const gss_buffer_t	srcBuf;
	gss_buffer_t *		destBuf;
	int			addNullChar;
{
	gss_buffer_t aBuf;
	int len;

	if (destBuf == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);

	*destBuf = 0;

	aBuf = (gss_buffer_t)malloc(sizeof (gss_buffer_desc));
	if (!aBuf)
		return (GSS_S_FAILURE);

	if (addNullChar)
		len = srcBuf->length + 1;
	else
		len = srcBuf->length;

	if (!(aBuf->value = (void*)malloc(len))) {
		free(aBuf);
		return (GSS_S_FAILURE);
	}


	memcpy(aBuf->value, srcBuf->value, srcBuf->length);
	aBuf->length = srcBuf->length;
	*destBuf = aBuf;

	/* optionally add a NULL character */
	if (addNullChar)
		((char *)aBuf->value)[aBuf->length] = '\0';

	return (GSS_S_COMPLETE);
} /* ****** __gss_create_copy_buffer  ****** */
