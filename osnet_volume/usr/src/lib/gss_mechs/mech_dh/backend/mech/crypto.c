/*
 *	crypto.c
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 */

#pragma ident	"@(#)crypto.c	1.4	98/06/11 SMI"

#include <sys/note.h>
#include "dh_gssapi.h"
#include "crypto.h"

/* Release the storage for a signature */
void
__free_signature(dh_signature_t sig)
{
	Free(sig->dh_signature_val);
	sig->dh_signature_val = NULL;
	sig->dh_signature_len = 0;
}

/* Release the storage for a gss_buffer */
void
__dh_release_buffer(gss_buffer_t b)
{
	Free(b->value);
	b->length = 0;
	b->value = NULL;
}

typedef struct cipher_entry {
	cipher_proc cipher;	/* Routine to en/decrypt with */
	unsigned int pad;	/* Padding need for the routine */
} cipher_entry, *cipher_t;

typedef struct verifer_entry {
	verifier_proc msg;	/* Routine to calculate the check sum */
	unsigned int size;	/* Size of check sum */
	cipher_t signer;	/* Cipher entry to sign the check sum */
} verifier_entry, *verifier_t;

typedef struct QOP_entry {
	int export_level;	/* Not currentlyt used */
	verifier_t verifier;	/* Verifier entry to use for integrity */
} QOP_entry;

/*
 * Return the length produced by using cipher entry c given the supplied len
 */
static unsigned int
cipher_pad(cipher_t c, unsigned int len)
{
	unsigned int pad;

	pad = c ? c->pad : 1;

	return (((len + pad - 1)/pad)*pad);
}




/*
 * DesN crypt packaged for use as a cipher entry
 */
static OM_uint32
__dh_desN_crypt(gss_buffer_t buf, dh_key_set_t keys, cipher_mode_t cipher_mode)
{
	int stat = DESERR_BADPARAM;

	if (DES_FAILED(stat))
		return (DH_CIPHER_FAILURE);

	return (DH_SUCCESS);
}

/*
 * Package up plain des cbc crypt for use as a cipher entry.
 */
static OM_uint32
__dh_des_crypt(gss_buffer_t buf, dh_key_set_t keys, cipher_mode_t cipher_mode)
{
	int stat = DESERR_BADPARAM;

	if (DES_FAILED(stat))
		return (DH_CIPHER_FAILURE);

	return (DH_SUCCESS);
}

/*
 * MD5_verifier: This is a verifier routine suitable for use in a
 * verifier entry. It calculates the MD5 check sum over an optional
 * msg and a token. It signs it using the supplied cipher_proc and stores
 * the result in signature.
 *
 * Note signature should already be allocated and be large enough to
 * hold the signature after its been encrypted. If keys is null, then
 * we will just return the unencrypted check sum.
 */
static OM_uint32
MD5_verifier(gss_buffer_t tok, /* The buffer to sign */
	    gss_buffer_t msg, /* Optional buffer to include */
	    cipher_proc signer, /* Routine to encrypt the integrity check */
	    dh_key_set_t keys, /* Optiona keys to be used with the above */
	    dh_signature_t signature /* The resulting MIC */)
{
	MD5_CTX md5_ctx;	/* MD5 context */
	gss_buffer_desc buf;	/* GSS buffer to hold keys for cipher routine */

	/* Initialize the MD5 context */
	MD5Init(&md5_ctx);
	/* If we have a message to digest, digest it */
	if (msg)
	    MD5Update(&md5_ctx, (unsigned char *)msg->value, msg->length);
	/* Digest the supplied token */
	MD5Update(&md5_ctx, (unsigned char *)tok->value, tok->length);
	/* Finalize the sum. The MD5 context contains the digets */
	MD5Final(&md5_ctx);

	/* Copy the digest to the signature */
	memcpy(signature->dh_signature_val, (void *)md5_ctx.digest, 16);

	buf.length = signature->dh_signature_len;
	buf.value = signature->dh_signature_val;

	/* If we have keys encrypt it */
	if (keys != NULL)
		return (signer(&buf, keys, ENCIPHER));

	return (DH_SUCCESS);
}

/* Cipher table */
static
cipher_entry cipher_tab[] = {
	{ NULL, 1},
	{ __dh_desN_crypt, 8},
	{ __dh_des_crypt, 8}
};


#define	__NO_CRYPT	&cipher_tab[0]
#define	__DES_N_CRYPT	&cipher_tab[1]
#define	__DES_CRYPT	&cipher_tab[2]

/* Verifier table */
static
verifier_entry verifier_tab[] = {
	{ MD5_verifier, 16, __DES_N_CRYPT },
	{ MD5_verifier, 16, __DES_CRYPT }
};

/* QOP table */
static
QOP_entry QOP_table[] = {
	{ 0, &verifier_tab[0] },
	{ 0, &verifier_tab[1] }
};

#define	QOP_ENTRIES (sizeof (QOP_table) / sizeof (QOP_entry))

/*
 * __dh_is_valid_QOP: Return true if qop is valid entry into the QOP
 * table, else return false.
 */
bool_t
__dh_is_valid_QOP(dh_qop_t qop)
{
	bool_t is_valid = FALSE;

	is_valid = qop < QOP_ENTRIES;

	return (is_valid);
}

/*
 * __alloc_sig: Allocate a signature for a given QOP. This takes into
 * account the size of the signature after padding for the encryption
 * routine.
 */
OM_uint32
__alloc_sig(dh_qop_t qop, dh_signature_t sig)
{
	OM_uint32 stat = DH_VERIFIER_FAILURE;
	verifier_entry *v;

	/* Check that the QOP is valid */
	if (!__dh_is_valid_QOP(qop))
		return (DH_UNKNOWN_QOP);

	/* Get the verifier entry from the QOP entry */
	v = QOP_table[qop].verifier;

	/* Calulate the length needed for the signature */
	sig->dh_signature_len = cipher_pad(v->signer, v->size);

	/* Allocate the signature */
	sig->dh_signature_val = (void*)New(char, sig->dh_signature_len);
	if (sig->dh_signature_val == NULL) {
		sig->dh_signature_len = 0;
		return (DH_NOMEM_FAILURE);
	}

	stat = DH_SUCCESS;

	return (stat);
}

/*
 * __get_sig_size: Return the total size needed for a signature given a QOP.
 */
OM_uint32
__get_sig_size(dh_qop_t qop, unsigned int *size)
{
	/* Check for valid QOP */
	if (__dh_is_valid_QOP(qop)) {
		/* Get the verifier entry */
		verifier_t v = QOP_table[qop].verifier;

		/* Return the size include the padding needed for encryption */
		*size = v ? cipher_pad(v->signer, v->size) : 0;

		return (DH_SUCCESS);
	}
	*size = 0;

	return (DH_UNKNOWN_QOP);
}

/*
 * __mk_sig: Generate a signature using a given qop over a token of a
 * given length and an optional message. We use the supplied keys to
 * encrypt the check sum if they are available. The output is place
 * in a preallocate signature, that was allocated using __alloc_sig.
 */
OM_uint32
__mk_sig(dh_qop_t qop, /* The QOP to use */
	char *tok, /* The token to sign */
	long len, /* The tokens length */
	gss_buffer_t mesg,	/* An optional message to be included */
	dh_key_set_t keys, /* The optional encryption keys */
	dh_signature_t sig /* The resulting MIC */)
{
	OM_uint32 stat = DH_VERIFIER_FAILURE;


	verifier_entry *v;	/* Verifier entry */
	gss_buffer_desc buf;	/* Buffer to package tok */

	/* Make sure the QOP is valid */
	if (!__dh_is_valid_QOP(qop))
		return (DH_UNKNOWN_QOP);

	/* Grab the verifier entry for the qop */
	v = QOP_table[qop].verifier;

	/* Package the token for use in a verifier_proc */
	buf.length = len;
	buf.value = tok;

	/*
	 * Calculate the signature using the supplied keys. If keys
	 * is null, the the v->signer->cipher routine will not be called
	 * and sig will not be encrypted.
	 */
	stat = (*v->msg)(&buf, mesg, v->signer->cipher, keys, sig);

	return (stat);
}

/*
 * __verify_sig: Verify that the supplied signature, sig, is the same
 * as the token verifier
 */
OM_uint32
__verify_sig(dh_token_t token, /* The token to be verified */
	    dh_qop_t qop, /* The QOP to use */
	    dh_key_set_t keys, /* The context session keys */
	    dh_signature_t sig /* The signature from the serialized token */)
{
	OM_uint32 stat = DH_VERIFIER_FAILURE;

	cipher_proc cipher;	/* cipher routine to use */
	gss_buffer_desc buf;	/* Packaging for sig */

	/* Check the QOP */
	if (!__dh_is_valid_QOP(qop))
		return (DH_UNKNOWN_QOP);

	/* Package up the supplied signature */
	buf.length = sig->dh_signature_len;
	buf.value = sig->dh_signature_val;

	/* Get the cipher proc to use from the verifier entry for qop */
	cipher = QOP_table[qop].verifier->signer->cipher;

	/* Encrypt the check sum using the supplied set of keys */
	if ((stat = (*cipher)(&buf, keys, ENCIPHER)) != DH_SUCCESS)
		return (stat);

	/* Compare the signatures */
	if (__cmpsig(sig, &token->verifier))
		return (DH_SUCCESS);

	stat = DH_VERIFIER_MISMATCH;

	return (stat);
}

/*
 * __cmpsig: Return true if two signatures are the same, else false.
 */
bool_t
__cmpsig(dh_signature_t s1, dh_signature_t s2)
{
	return (s1->dh_signature_len == s2->dh_signature_len &&
	    memcmp(s1->dh_signature_val,
		s2->dh_signature_val, s1->dh_signature_len) == 0);
}

/*
 * wrap_msg_body: Wrap the message pointed to be in into a
 * message pointed to by out that has ben padded out by pad bytes.
 *
 * The output message looks like:
 * out->length = total length of out->value including any padding
 * out->value points to memory as follows:
 * +------------+-------------------------+---------|
 * | in->length | in->value               | XDR PAD |
 * +------------+-------------------------+---------|
 *    4 bytes      in->length bytes         0 - 3
 */
static OM_uint32
wrap_msg_body(gss_buffer_t in, gss_buffer_t out)
{
	XDR xdrs;			/* xdrs to wrap with */
	unsigned int len, out_len;	/* length  */
	size_t size;

	out->length = 0;
	out->value = 0;

	/* Make sure the address of len points to a 32 bit word */
	len = (unsigned int)in->length;
	if (len != in->length)
		return (DH_ENCODE_FAILURE);

	size = ((in->length + sizeof (OM_uint32) + 3)/4) * 4;
	out_len = size;
	if (out_len != size)
		return (DH_ENCODE_FAILURE);

	/* Allocate the output buffer and set the length */
	if ((out->value = (void *)New(char, len)) == NULL)
		return (DH_NOMEM_FAILURE);
	out->length = out_len;


	/* Create xdr stream to wrap into */
	xdrmem_create(&xdrs, out->value, out->length, XDR_ENCODE);

	/* Wrap the bytes in value */
	if (!xdr_bytes(&xdrs, (char **)&in->value, &len, len)) {
		__dh_release_buffer(out);
		return (DH_ENCODE_FAILURE);
	}

	return (DH_SUCCESS);
}

/*
 * __QOPSeal: Wrap the input message placing the output in output given
 * a valid QOP. If confidentialiy is requested it is ignored. We can't
 * support privacy. The return flag will always be zero.
 */
OM_uint32
__QOPSeal(dh_qop_t qop, /* The QOP to use */
	gss_buffer_t input, /* The buffer to wrap */
	int conf_req, /* Do we want privacy ? */
	dh_key_set_t keys, /* The session keys */
	gss_buffer_t output, /* The wraped message */
	int *conf_ret /* Did we encrypt it? */)
{
_NOTE(ARGUNUSED(conf_req,keys))
	OM_uint32 stat = DH_CIPHER_FAILURE;

	*conf_ret = FALSE;	/* No encryption allowed */

	/* Check for valid QOP */
	if (!__dh_is_valid_QOP(qop))
		return (DH_UNKNOWN_QOP);

	/* Wrap the message */
	if ((stat = wrap_msg_body(input, output))
	    != DH_SUCCESS)
		return (stat);

	return (stat);
}

/*
 * unwrap_msg_body: Unwrap the message, that was wrapped from above
 */
static OM_uint32
unwrap_msg_body(gss_buffer_t in, gss_buffer_t out)
{
	XDR xdrs;
	unsigned int len;	/* sizeof (len) == 32bits */

	/* Create an xdr stream to on wrap in */
	xdrmem_create(&xdrs, in->value, in->length, XDR_DECODE);

	/* Unwrap the input into out->value */
	if (!xdr_bytes(&xdrs, (char **)&out->value, &len, in->length))
		return (DH_DECODE_FAILURE);

	/* set the length */
	out->length = len;

	return (DH_SUCCESS);
}

/*
 * __QOPUnSeal: Unwrap the input message into output using the supplied QOP.
 * Note it is the callers responsibility to release the allocated output
 * buffer. If conf_req is true we return DH_CIPHER_FAILURE since we don't
 * support privacy.
 */
OM_uint32
__QOPUnSeal(dh_qop_t qop, /* The QOP to use */
	    gss_buffer_t input, /* The message to unwrap */
	    int conf_req, /* Is the message encrypted */
	    dh_key_set_t keys, /* The session keys to decrypt if conf_req */
	    gss_buffer_t output /* The unwraped message */)
{
_NOTE(ARGUNUSED(keys))
	OM_uint32 stat = DH_CIPHER_FAILURE;

	/* Check that the qop is valid */
	if (!__dh_is_valid_QOP(qop))
		return (DH_UNKNOWN_QOP);

	/* Set output to sane values */
	output->length = 0;
	output->value = NULL;

	/* Fail if this is privacy */
	if (conf_req)
		return (DH_CIPHER_FAILURE);

	/* Unwrap the input into the output, return the status */
	stat = unwrap_msg_body(input, output);

	return (stat);
}
