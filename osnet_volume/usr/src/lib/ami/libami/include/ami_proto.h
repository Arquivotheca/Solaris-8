/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_AMI_PROTO_H
#define	_AMI_PROTO_H

#pragma ident "@(#)ami_proto.h	1.3 99/07/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <rpc/rpc.h>
#include "ami.h"
#include "ami_local.h"

/* Initial AMI APIs */
AMI_STATUS ami_gen_des_key PROTO_LIST((
	const ami_handle_t,	/* IN:	ami handle */
	uchar_t **,		/* OUT: DES session key */
	ami_alg_params **));	/* OUT: IV */

AMI_STATUS ami_gen_des3_key PROTO_LIST((
	const ami_handle_t,	/* IN:	ami handle */
	uchar_t **,		/* OUT: triple DES session key */
	ami_alg_params **));	/* OUT: IV */

AMI_STATUS ami_gen_rc2_key PROTO_LIST((
	const ami_handle_t,	/* IN:  AMI handle */
	const size_t,		/* IN:  key length */
	const uint_t,		/* IN:  effective key size in bits */
	uchar_t **,		/* OUT: RC2 session key */
	ami_alg_params **));	/* OUT: RC2 parameter */

AMI_STATUS ami_gen_rc4_key PROTO_LIST((
	const ami_handle_t,	/* IN:	ami handle */
	const size_t,		/* IN:  key length in bytes */
	uchar_t **));		/* OUT: RC4 key */

AMI_STATUS ami_gen_rsa_keypair PROTO_LIST((
	const ami_handle_t,		/* IN:	ami handle */
	const ami_rsa_keygen_param *,	/* IN:  keypair gen parameters */
	const uchar_t *,
	const size_t,
	uchar_t **,			/* OUT: public key */
	size_t *,			/* OUT: public key length */
	uchar_t **,			/* OUT: private key */
	size_t *));			/* OUT: private key length */

/* DN */
AMI_STATUS ami_str2dn PROTO_LIST((
	const ami_handle_t, char *, ami_name **
));

AMI_STATUS ami_dn2str PROTO_LIST((
	const ami_handle_t, ami_name *, char **
));

/* Supported algorithms */
AMI_STATUS ami_get_alglist PROTO_LIST((ami_alg_list **));

AMI_STATUS ami_set_keypkg PROTO_LIST((
	const ami_handle_t,	/* IN: ami handle */
	const char *,		/* IN: keypkg filename or repository index */
	const ami_keypkg *));	/* IN: keypkg to be stored */

AMI_STATUS ami_set_cert PROTO_LIST((
	const ami_handle_t,	/* IN: ami handle */
	const char *,		/* IN: cert filename or repository index */
	const ami_cert *));	/* IN: certificate */

AMI_STATUS ami_verify_cert_chain(
	const ami_handle_t,		/* IN: ami handle */
	const ami_cert *, 	/* IN: certificate chain to be verified */
	const int,			/* IN: length of cert chain */
	const struct ami_tkey_list *,	/* IN: trusted key list */
	const int,			/* IN: flags (unused) */
	ami_cert **);		/* OUT: first expired certificate */

AMI_STATUS ami_verify_cert_est_chain(
	const ami_handle_t,		/* IN: ami handle */
	const ami_cert *, 		/* IN: certificate to be verified */
	const struct ami_tkey_list *,	/* IN: trusted key list */
	const char **,			/* IN: CA Name list */
	const int,			/* IN: flags (unused) */
	ami_cert **,			/* OUT: first expired certificate */
	ami_cert **,			/* OUT: certificate chain */
	int *);				/* OUT: length of cert chain */

AMI_STATUS ami_set_cert(
	const ami_handle_t,	/* IN: ami handle */
	const char *,		/* IN: cert filename or repository index */
	const ami_cert *);	/* IN: certificate */

/* Free DN */
void ami_free_dn PROTO_LIST((ami_name **));

/* DN */
AMI_STATUS ami_str2dn PROTO_LIST((
	const ami_handle_t, char *, ami_name **));
AMI_STATUS ami_dn2str PROTO_LIST((
	const ami_handle_t, ami_name *, char **));

/* Supported algorithms */
AMI_STATUS ami_get_alglist PROTO_LIST((ami_alg_list **));

/*-----------------------------------------------------------------------*/

/* Retrieval from/Storage to XFN */
AMI_STATUS __ami_set_keypkg PROTO_LIST((
	const ami_handle *,	/* IN: ami handle */
	const int,		/* IN: flags - how to store keypkg */
	const char *,		/* IN: keypkg filename or repository index */
	const ami_keypkg *));	/* IN: keypkg to be stored */
AMI_STATUS __ami_get_keypkg PROTO_LIST((
	const ami_handle *,	/* IN:	ami handle */
	const int,		/* IN:  flags - how to get keypkg */
	const char *,		/* IN:  keypkg_filename or repository index */
	ami_keypkg **));		/* OUT: keypkg */
AMI_STATUS __ami_set_cert PROTO_LIST((
	const ami_handle *,	/* IN: ami handle */
	const int,		/* IN: flags - how to store cert */
	const char *,		/* IN: cert filename or repository index */
	const ami_cert *));	/* IN: certificate */
AMI_STATUS __ami_get_cert PROTO_LIST((
	const ami_handle *,	/* IN:	ami handle */
	const int,		/* IN:  flags - how to get cert */
	const char *,		/* IN:  certificate filename, rep index, DN */
	ami_cert **,		/* OUT: set of certificates */
	int *));

/*
 * Local prototypes
 */
AMI_STATUS __ami_init PROTO_LIST((
        ami_handle **,
        const char *,
        const char *,
        const u_int,
        const u_int,
        const char *));

AMI_STATUS __ami_dn2str PROTO_LIST((
	const ami_handle *, ami_name *, int, char **));

AMI_STATUS __ami_verify_certreq PROTO_LIST((
	const ami_handle *,	/* IN: ami handle */
	ami_certreq_ptr, 		/* IN: PKCS #10 certificate request */
	ami_pubkey_info_ptr		/* IN: public verification key */
));

AMI_STATUS __ami_verify_netscape_certreq PROTO_LIST((
	const ami_handle *amih,	/* IN: ami handle */
	ami_signed_pubkey_and_challenge *pCertReq,	/* IN */
	ami_pubkey_info_ptr pKeyInfo	/* IN: public verification key */
));

AMI_STATUS __ami_verify_keypkg PROTO_LIST((
	const ami_handle *,	/* IN: AMI handle */
	ami_keypkg *,		/* IN: keypkg to be verified */
	ami_pubkey_info_ptr	/* IN: verification key */
));

AMI_STATUS ami_verify_keypkg_return_digest PROTO_LIST((
	const ami_handle *,	/* IN: ami handle */
	ami_keypkg *,		/* IN: keypkg to be verified */
	ami_pubkey_info_ptr,	/* IN: public verification key */
	uchar_t **, 		/* OUT: key package digest */
	size_t * 		/* OUT: key package digest length */
));

AMI_STATUS __ami_sign1 PROTO_LIST((
	ami_handle *,		/* IN:  ami handle */
	uchar_t *, 		/* IN:  data to be signed */
	u_int,			/* IN:  data length */
	const ami_algid *pKeyAlg, /* IN:  signature key algorithm */
	uchar_t *,		/* IN:  signature key */
	u_int,			/* IN:  signature key length */
	const ami_algid *pSigAlg, /* IN:  signature algorithm */
	uchar_t **,		/* OUT: signature */
	u_int *			/* OUT: signature length */
));

AMI_STATUS __ami_sign_certreq PROTO_LIST((
	ami_handle *,		/* IN:	ami handle */
	ami_certreq_ptr,	/* IN:  PKCS #10 certificate request */
	ami_algid_ptr,		/* IN:  signature key algorithm */
	uchar_t *,		/* IN:  signature key */
	u_int,			/* IN:  signature key length */
	ami_algid_ptr		/* IN:  signature algorithm */
));

AMI_STATUS __ami_sign_cert PROTO_LIST((
	ami_handle *,		/* IN:	ami handle */
	ami_cert_ptr,		/* IN:  certificate to be signed */
	ami_algid_ptr,		/* IN:  signature key algorithm */
	uchar_t *,		/* IN:  signature key */
	u_int,  		/* IN:  signature key length */
	ami_algid_ptr		/* IN:  signature algorithm */
));

AMI_STATUS __ami_sign_keypkg PROTO_LIST((
	ami_handle *,		/* IN:	ami handle */
	ami_keypkg *,		/* IN:  keypkg to be signed */
	ami_algid_ptr,		/* IN:  signature key algorithm */
	uchar_t *,		/* IN:  signature key */
	u_int,			/* IN:  signature key length */
	ami_algid_ptr		/* IN:  signature algorithm */
));

AMI_STATUS __ami_gen_des_key_from_pwd PROTO_LIST((
	const ami_handle *, const ami_algid *, const char *,
	uchar_t **, ami_alg_params **));

AMI_STATUS __ami_gen_des3_key_from_pwd PROTO_LIST((
	const ami_handle *,	/* IN:	ami handle */
	const ami_des_keygen_param *, /* IN:  key generation parameters */
	uchar_t **, 		/* OUT: secret key */
	u_int *,		/* OUT: secret key length */
	uchar_t **, 		/* OUT: IV */
	u_int *			/* OUT: IV length */
));

AMI_STATUS __ami_get_keypkg_data_set PROTO_LIST((
	const ami_handle *,	/* IN:	ami handle */
	int,			/* IN:  flags = how to get keypkg */
	char *,			/* IN:  keypkg_filename or repository index */
	ami_data_set **,	/* OUT: array of encrypted keypkgs */
	int *			/* OUT: array length of keypkg data */

));

AMI_STATUS __ami_check_keypkg_presence PROTO_LIST((
	ami_handle *,		/* IN:	ami handle */
	char *,			/* IN:	keypkg ID string */
	int,			/* IN:  how to get keypkg */
	char *,			/* IN:	keypkg filename or rep index */
	int *			/* OUT:	keypkg presence */
));

AMI_STATUS __ami_validate_keypkg_with_privkey PROTO_LIST((
	const ami_handle *,	/* IN:  ami handle */
	const uchar_t *,	/* IN:  ASN.1 encoded keypkg */
	const size_t,		/* IN:  length of ASN.1 stream */
	const uchar_t *,	/* IN:  private key */
	const size_t,		/* IN:  private key length */
	const uchar_t *,	/* IN:  private key algorithm */
	const u_int		/* IN:  private key algorithm len */
));

AMI_STATUS __ami_verify_keypkg_sig PROTO_LIST((
	const ami_handle *,	/* IN:	ami handle */
	uchar_t *,		/* IN:  ASN.1 encoded keypkg */
	u_int,			/* IN:  length of ASN.1 stream */
	ami_keypkg **		/* OUT: keypkg */
));

AMI_STATUS __ami_validate_keypkg_with_pwd PROTO_LIST((
	const ami_handle *,	/* IN:	ami handle */
	uchar_t *,		/* IN:  ASN.1 encoded keypkg */
	u_int,			/* IN:  length of ASN.1 stream */
	char *,			/* IN:	passphrase to decrypt keypkg */
	ami_keypkg **,		/* OUT: keypkg */
	uchar_t **, 		/* OUT: decrypted private key */
	u_int * 		/* OUT: length of decrypted private key */
));

AMI_STATUS __ami_get_cert_data_set PROTO_LIST((
	const ami_handle *,	/* IN:	ami handle */
	int,			/* IN:	where to get keypkg */
	char *,			/* IN:	keypkg filename or rep index */
	ami_data_set **,	/* OUT:	array of keypkgs returned */
	int *
));

AMI_STATUS __ami_remove_cert PROTO_LIST((
	ami_handle *,		/* IN: ami handle */
	int,			/* IN: how go get certs */
	char *,			/* IN: cert filename, rep index, DN */
	ami_name_ptr,		/* IN: issuer name */
	ami_cert_serialnum *	/* IN: serial number */
));

AMI_STATUS __ami_get_rsa_keysize PROTO_LIST((
	ami_handle *,		/* IN:  AMI handle */
	u_int,			/* IN:	AMI_RSA_PUBLIC || AMI_RSA_PRIVATE */
	uchar_t *,		/* IN:  PKCS#1 encoded private key */
	u_int,			/* IN:  private key length */
	u_int *			/* OUT: modulus size */
));

AMI_STATUS __ami_encrypt_private_key PROTO_LIST((
	ami_handle *,		/* IN:	ami handle */
	uchar_t *,		/* IN:	private key */
	size_t,			/* IN:	private key length */
	uchar_t *,		/* IN:	encryption key */
	size_t,			/* IN:	encryption key length */
	ami_octetstring *,		/* IN:  iv */
	uchar_t **,		/* OUT: ciphertext */
	size_t *		/* OUT: ciphertext length */
));

AMI_STATUS __ami_decrypt_private_key PROTO_LIST((
	ami_handle *,		/* IN:	ami handle */
	uchar_t *,		/* IN:	encrypted private key */
	size_t,			/* IN:	private key length */
	uchar_t *,		/* IN:	decryption key */
	size_t,			/* IN:	decryption key length */
	ami_octetstring *,		/* IN:  iv */
	uchar_t **,		/* OUT: cleartext */
	size_t *		/* OUT: cleartext length */
));

AMI_STATUS __ami_decrypt PROTO_LIST((
	ami_handle *,	/* IN:	ami handle */
	const uchar_t *,	/* IN:	input data */
	const size_t,	/* IN:	input data length */
	const int,		/* IN:	more input data flag */
	const ami_algid *,	/* IN:	encryption key algorithm */
	const uchar_t *,	/* IN:	encryption key */
	const size_t,	/* IN:	encryption key length */
	const ami_algid *,	/* IN:	encryption algorithm */
	uchar_t **,	/* OUT: cleartxt */
	size_t *	/* IN/OUT: cleartxt length */
));

AMI_STATUS ___ami_decrypt PROTO_LIST((
        ami_handle *, /* IN:  ami handle */
        const uchar_t *,        /* IN:  input data */
        const size_t,   /* IN:  input data length */
        const int,              /* IN:  more input data flag */
        const ami_algid *,      /* IN:  encryption key algorithm */
        const uchar_t *,        /* IN:  encryption key */
        const size_t,   /* IN:  encryption key length */
        const ami_algid *,      /* IN:  encryption algorithm */
	const int effectiveKeySize,
        uchar_t **,     /* OUT: cleartxt */
        size_t *        /* IN/OUT: cleartxt length */
));

AMI_STATUS ___ami_encrypt PROTO_LIST((
	ami_handle *,	/* IN:	ami handle */
	const uchar_t *,	/* IN:	input data */
	const size_t,	/* IN:	input data length */
	const int,		/* IN:	more input data flag */
	const ami_algid *,	/* IN:	encryption key algorithm */
	const uchar_t *,	/* IN:	encryption key */
	const size_t,	/* IN:	encryption key length */
	const ami_algid *,	/* IN:	encryption algorithm */
	const int effectiveKeySize,
	uchar_t **,	/* OUT: ciphertext */
	size_t *	/* IN/OUT: ciphertext length */
));
AMI_STATUS __ami_encrypt PROTO_LIST((
	ami_handle *,	/* IN:	ami handle */
	const uchar_t *,	/* IN:	input data */
	const size_t,	/* IN:	input data length */
	const int,		/* IN:	more input data flag */
	const ami_algid *,	/* IN:	encryption key algorithm */
	const uchar_t *,	/* IN:	encryption key */
	const size_t,	/* IN:	encryption key length */
	const ami_algid *,	/* IN:	encryption algorithm */
	uchar_t **,	/* OUT: ciphertext */
	size_t *	/* IN/OUT: ciphertext length */
));

AMI_STATUS __ami_wrap_key PROTO_LIST((
	const ami_handle *,	/* IN: AMI handle */
	const uchar_t *,	/* IN: key to be wrapped  */
	const size_t,		/* IN: length of key to be wrapped */
	const ami_algid *,	/* IN: wrapping key algorithm */
	const uchar_t *,	/* IN: wrapping key */
	const size_t,		/* IN: wrapping key length */
	const ami_algid *,	/* IN: wrapping algorithm */
	uchar_t **,		/* OUT:    wrapped key */
	size_t *		/* IN/OUT: wrapped key length */
));

AMI_STATUS __ami_create_pkcs7_signedData PROTO_LIST((
	ami_handle *,		/* IN:  ami handle */
	const uchar_t *, 	/* IN:  data to be signed */
	const size_t,		/* IN:  data length */
	const ami_algid *,	/* IN:  key algorithm */
	const uchar_t *,	/* IN:  signature key */
	const size_t,		/* IN:  signature key length */
	const ami_algid *,	/* IN:  signature algorithm */
	const ami_cert *,	/* IN:  certificate */
	const char **,		/* IN:  list of CAs */
	const int,		/* IN:  flag  */
	uchar_t **,		/* OUT: PKCS#7 signed data */
	size_t *   		/* OUT: PKCS#7 signed data len */
));

AMI_STATUS __ami_verify_pkcs7_signedData PROTO_LIST((
	ami_handle *,		/* IN: AMI handle */
	const uchar_t *,	/* IN: PKCS#7 signed data */
	const size_t,		/* IN: PKCS#7 signed data len */
	const struct ami_tkey_list *, /* IN: trusted public keys */
	const char **,		/* IN: list of CAs */
	const int,		/* IN: flag */
	uchar_t **,		/* OUT: verified data */
	size_t *,   		/* OUT: verified data length */
	char **,		/* OUT: signer's name */
	ami_cert **,		/* OUT: cert chain */
	int *,			/* OUT: number of certs in chain */
	ami_cert **		/* OUT: first expired cert */
));


/* AMI utility functions */
AMI_STATUS __ami_find_dn_in_list PROTO_LIST((char **, ami_name *));
AMI_STATUS __ami_get_keypkg_with_pwd PROTO_LIST((
	const ami_handle *, int, char *, char *,
	ami_keypkg **, uchar_t **, u_int *));
AMI_STATUS __ami_remove_keypkg PROTO_LIST((
	const ami_handle *, int, char *));
AMI_STATUS __ami_get_encryption_phrase PROTO_LIST((
	char *, int, char **));
AMI_STATUS __ami_dump_output PROTO_LIST((
	char *, ami_data_format, uchar_t *, u_int, int, int));
AMI_STATUS __ami_fdump_output PROTO_LIST((
	FILE *, ami_data_format, uchar_t *, u_int,
	int, int));
AMI_STATUS __ami_dump_append_and_delimit PROTO_LIST((
	int, char *, ami_data_format, uchar_t *, u_int));
AMI_STATUS __ami_dump_append PROTO_LIST((
	char *, ami_data_format, uchar_t *, u_int));
AMI_STATUS __ami_dump_keypkg PROTO_LIST((
	ami_handle *, char *, ami_keypkg_ptr, int));
AMI_STATUS __ami_dump_cert PROTO_LIST((
	ami_handle *, char *, ami_cert_ptr, int, ami_data_format));
AMI_STATUS __ami_dump_certreq PROTO_LIST((
	ami_handle *, char *, ami_certreq_ptr, int, int));
AMI_STATUS __ami_dump_data_set PROTO_LIST((
	int, char *, ami_data_format, int, ami_data_set *));
AMI_STATUS __ami_format_output PROTO_LIST((
	uchar_t *, u_int, int, int, uchar_t **, u_int *));
AMI_STATUS __ami_read_input PROTO_LIST((
	char *, ami_data_format, uchar_t **, u_int *));
AMI_STATUS __ami_change_pass_phrase PROTO_LIST((
	ami_handle *, char *, uchar_t *, u_int, ami_keypkg_ptr));
AMI_STATUS __ami_get_fname PROTO_LIST((char *, char **));
AMI_STATUS __ami_parse_hex PROTO_LIST((uchar_t *, u_int, uchar_t **));
AMI_STATUS __ami_file_2_certlist PROTO_LIST((
	ami_handle *, char *, item_list **));
void __ami_print PROTO_LIST((FILE *, int, int, char *, ...));
AMI_STATUS __ami_snprintf PROTO_LIST((char *, size_t, const char *, ...));

/*
 * Free
 *
 * ami_free_keypkg, ami_free_cert, and ami_free_dn are in ami.h
 */
void __ami_free2_dn PROTO_LIST((ami_name_ptr));
void __ami_free2_cert PROTO_LIST((ami_cert_ptr));
void __ami_free_validity PROTO_LIST((ami_validity_ptr *));
void __ami_free2_validity PROTO_LIST((ami_validity_ptr));
void __ami_free_algid PROTO_LIST((ami_algid_ptr *));
void __ami_free2_algid PROTO_LIST((ami_algid_ptr));
void __ami_free_certreq PROTO_LIST((ami_certreq_ptr *));
void __ami_free_keyinfo PROTO_LIST((ami_pubkey_info_ptr *));
void __ami_free2_keyinfo PROTO_LIST((ami_pubkey_info_ptr));
void __ami_free_trusted_key PROTO_LIST((ami_tkey_ptr *));
void __ami_free2_trusted_key PROTO_LIST((ami_tkey_ptr));
void __ami_free_cert_pair PROTO_LIST((ami_cert_pair_ptr *));
void __ami_free_pkcs_content_info PROTO_LIST((struct ami_content_info **));
void __ami_free_digest_info PROTO_LIST((ami_digest_info_ptr *));
void __ami_free_ostr(ami_octetstring **ppOstr);
void __ami_free_bstr PROTO_LIST((ami_bitstring **ppBstr));
void __ami_free_uid PROTO_LIST((struct ami_uid **));
void __ami_free_oid PROTO_LIST((ami_oid **ppOid));
void __ami_free_serial PROTO_LIST((ami_cert_serialnum **));
void __ami_free_rcpt_info PROTO_LIST((struct ami_rcpt_info **));
void __ami_free_rcpt_infos PROTO_LIST((struct ami_rcpt_info_list **));
void __ami_free_rdn PROTO_LIST((struct ami_rdname **));
void __ami_free_rdn_seq PROTO_LIST((struct ami_rdn_seq **));
void __ami_free_ava PROTO_LIST((struct ami_ava **));
void __ami_free_item_list PROTO_LIST((set_item **));
void __ami_free_encrypted_private_key_info PROTO_LIST((
	ami_encr_privkey_info **));
void __ami_free2_encrypted_private_key_info PROTO_LIST
	((ami_encr_privkey_info *));
void __ami_free_keyinfo_pdu PROTO_LIST((ami_handle *, ami_pubkey_info_ptr));
void __ami_free_content_info_fm_pdu PROTO_LIST((ami_handle *,
	struct ami_content_info_fm *));
void __ami_free_envdata_pdu PROTO_LIST((ami_handle *,
	struct ami_enveloped_data *));
void __ami_free_encrdata_pdu PROTO_LIST((ami_handle *,
	struct ami_encr_data *));
void __ami_free_sigdata_fm_pdu PROTO_LIST((ami_handle *,
	struct ami_signed_data_fm *));
void __ami_free_pkcsdata_fm_pdu PROTO_LIST((ami_handle *,
	ami_pkcs_data_fm *));
void __ami_free_pkcsdata_pdu PROTO_LIST((ami_handle *,
	ami_pkcs_data *));
void __ami_free_rc2_cbc_params_pdu PROTO_LIST((
	ami_handle *, ami_rc2_cbc_param *));
void __ami_free_encrypted_private_key_info_pdu PROTO_LIST((
	ami_handle *, ami_encr_privkey_info *));
void __ami_free_netscape_certreq PROTO_LIST((
	ami_handle *,
	ami_signed_pubkey_and_challenge *));
void __ami_free_private_key_info_pdu PROTO_LIST((
	ami_handle *, ami_privkey_info *));
void __ami_free_pbe_paramater_pdu PROTO_LIST((ami_handle *,
	ami_pbe_param *));
void __ami_free_v3_extension PROTO_LIST((ami_cert_extn_ptr *));
void __ami_free2_v3_extension PROTO_LIST((ami_cert_extn_ptr));
void __ami_free_v3_cert_extensions PROTO_LIST((struct ami_cert_extn_list **));

/* Compare */
AMI_STATUS __ami_cmp_algid PROTO_LIST((
	const ami_algid_ptr, const ami_algid_ptr, int *));
AMI_STATUS __ami_cmp_keyinfo PROTO_LIST((
	const ami_pubkey_info_ptr, const ami_pubkey_info_ptr, int *));
AMI_STATUS __ami_cmp_serial PROTO_LIST((
	const ami_cert_serialnum *,
	const ami_cert_serialnum *, int *));
AMI_STATUS __ami_cmp_oid PROTO_LIST((
	const ami_oid *, const ami_oid *, int *));
AMI_STATUS __ami_cmp_bstr PROTO_LIST((
	const ami_bitstring *, const ami_bitstring *, int *));
AMI_STATUS __ami_cmp_dn PROTO_LIST((
	const ami_handle *, const ami_name_ptr, const ami_name_ptr, int *));

/* Copy */
AMI_STATUS __ami_copy_ostr PROTO_LIST((ami_octetstring **, ami_octetstring *));
AMI_STATUS __ami_copy2_ostr PROTO_LIST((ami_octetstring *, ami_octetstring *));
AMI_STATUS __ami_copy_cert PROTO_LIST((ami_cert_ptr *, ami_cert_ptr));
AMI_STATUS __ami_copy2_cert PROTO_LIST((ami_cert_ptr, ami_cert_ptr));
AMI_STATUS __ami_copy_keyinfo PROTO_LIST((ami_pubkey_info_ptr *,
	ami_pubkey_info_ptr));
AMI_STATUS __ami_copy_algid PROTO_LIST((ami_algid_ptr *, ami_algid_ptr));
AMI_STATUS __ami_copy_tkey PROTO_LIST((ami_tkey_ptr *, ami_tkey_ptr));
AMI_STATUS __ami_copy_oid PROTO_LIST((ami_oid **, ami_oid *));
AMI_STATUS __ami_copy_bstr PROTO_LIST((struct ami_bitstring **, struct ami_bitstring *));
AMI_STATUS copy_serial PROTO_LIST((ami_cert_serialnum **,
	ami_cert_serialnum *));
AMI_STATUS __ami_copy2_serial PROTO_LIST((ami_cert_serialnum *,
	ami_cert_serialnum *));
AMI_STATUS __ami_copy_dn PROTO_LIST((ami_name_ptr *, ami_name_ptr));
AMI_STATUS __ami_copy2_dn PROTO_LIST((ami_name_ptr, ami_name_ptr));
AMI_STATUS __ami_copy_encrypted_private_key_info PROTO_LIST((
	ami_encr_privkey_info **, ami_encr_privkey_info *));
AMI_STATUS __ami_copy_v3_cert_extensions PROTO_LIST((
	struct ami_cert_extn_list **, struct ami_cert_extn_list *));

/* Print */
AMI_STATUS __ami_fprint_keypkg PROTO_LIST((
	ami_handle *, FILE *, ami_keypkg *, int, int));
AMI_STATUS __ami_fprint_cert PROTO_LIST((
	ami_handle *, FILE *, ami_cert_ptr, int, int));
AMI_STATUS __ami_fprint_certreq PROTO_LIST((
	ami_handle *, FILE *, ami_certreq_ptr, int, int));
AMI_STATUS __ami_fprint_netscape_certreq PROTO_LIST((
	ami_handle *, FILE *, ami_signed_pubkey_and_challenge *,
	int, int));
AMI_STATUS __ami_fprint_cert_log PROTO_LIST((
	ami_handle *, FILE *, ami_cert_ptr, int));

/* Time */
AMI_STATUS __ami_printable_time PROTO_LIST((UTCTime *, char **));
AMI_STATUS __ami_set_validity PROTO_LIST((long, long, ami_validity_ptr *));
AMI_STATUS utctime2time_t PROTO_LIST((UTCTime *, time_t *));
AMI_STATUS __ami_is_current PROTO_LIST((ami_validity_ptr));
AMI_STATUS __ami_get_current_time PROTO_LIST((char **));

/* Misc */
AMI_STATUS __ami_alg2ParmType PROTO_LIST
	((const ami_oid *, ami_parm_type *));
AMI_STATUS __ami_alg2AlgType PROTO_LIST((const ami_oid *, ami_alg_type *));
AMI_STATUS __ami_alg2name PROTO_LIST((const ami_oid *, char **));
AMI_STATUS __ami_name2alg PROTO_LIST((const char *, ami_oid **));
AMI_STATUS __ami_algname2ExportKeySize PROTO_LIST((
	const char *, size_t *));
AMI_STATUS __ami_attrtype2name PROTO_LIST((const ami_oid *, char **));
AMI_STATUS __ami_str_up PROTO_LIST((char *, int, char **));
char * __ami_getIPaddress PROTO_LIST(());

/* Serial */
AMI_STATUS __ami_gen_serial PROTO_LIST((ami_handle *, uchar_t **, u_short *));

/* Conversion */
AMI_STATUS __ami_binary_2_rfc1421 PROTO_LIST((
	uchar_t *, u_int, uchar_t **, u_int *));
AMI_STATUS __ami_rfc1421_2_binary PROTO_LIST((
	uchar_t *, u_int, uchar_t **, u_int *));
AMI_STATUS binary_2_hex PROTO_LIST((
	uchar_t *, u_int, uchar_t **));
AMI_STATUS __ami_hex_2_binary PROTO_LIST((uchar_t *, uchar_t **, u_int *));
AMI_STATUS __ami_binary_2_rfc1421_files PROTO_LIST((char *, char *, int));
AMI_STATUS __ami_rfc1421_2_binary_files PROTO_LIST((char *, char *, int));
AMI_STATUS __ami_add_linewraps PROTO_LIST((int, char *, char *, int));

/* Trusted keys */
AMI_STATUS __ami_add_trusted_key PROTO_LIST((
	const ami_handle *,		/* IN:  AMI handle */
	ami_tkey_ptr,			/* IN:  new trusted key */
	struct ami_tkey_list **));	/* IN/OUT: trusted keys */
AMI_STATUS __ami_delete_trusted_key PROTO_LIST((
	const ami_handle *,		/* IN:  AMI handle */
	ami_tkey_ptr,			/* IN:  trusted key info */
	struct ami_tkey_list **));	/* OUT: trusted keys */
AMI_STATUS __ami_is_trusted_key PROTO_LIST((
	const ami_handle *,		/* IN: AMI handle */
	struct ami_tkey_list *,	/* IN: list of trusted keys */
	ami_cert_ptr));			/* IN: certificate */

/*
 * DN
 *
 * ami_str2dn and ami_dn2str are in ami.h
 */
AMI_STATUS __ami_normalize_DN PROTO_LIST((
	ami_handle *, char *, int, char **));


/*
 * RPC
 */
AMI_STATUS __ami_rpc_get_amisev_client_handle(CLIENT **clnt);

AMI_STATUS __ami_rpc_ami_random_gen(int length, uchar_t **random);

AMI_STATUS __ami_rpc_set_keystore(CLIENT *cl, const char *username,
    const char *hostname, const char *hostip, long uid, const char *password,
    const char *rsaSignAlias, const char *rsaEncyAlias,	const char *dsaAlias,
    int flag, const void *data,	int data_len);

AMI_STATUS __ami_rpc_change_keystore_password(CLIENT *cl, const void *olddata,
    int olddata_len, const char *oldpasswd, const char *newpasswd,
    void **keystore, int *keystore_len);

AMI_STATUS __ami_rpc_sign_data(CLIENT *cl, const char *username,
    const char *hostname, const char *hostip, long uid, const char *keyalias,
    const char *algorithm, const void *data, int data_len, void **outSignData,
    int *outSignData_len);

AMI_STATUS __ami_rpc_unwrap_data(CLIENT *cl, const char *username,
    const char *hostname, const char *hostip, long uid, const char *keyalias,
    const char *algorithm, const void *data, int data_len, void **outData,
    int *outData_len);

AMI_STATUS __ami_rpc_get_keystore(CLIENT *cl, const char *username,
    const char *hostname, const char *hostip, long uid,
    void **keystore, int *keystore_len);

AMI_STATUS __ami_rpc_get_tursted_certificates(CLIENT *cl,
    const char *username, const char *hostip, long uid,
    ami_cert_list_t *certs);

AMI_STATUS __ami_rpc_encode_digested_data(uchar_t *data,
    size_t length,
    uchar_t *algo,
    uchar_t **paramValue,
    size_t *paramLength);

AMI_STATUS __ami_rpc_get_amiserv_client_handle(CLIENT **clnt);

AMI_STATUS __ami_rpc_decode_octect_string(uchar_t * encodedData,
    size_t length,
    ami_octetstring **pIv);

AMI_STATUS __ami_rpc_encode_octect_string(ami_octetstring *iv,
    uchar_t **paramValue,
    size_t *paramLength);

AMI_STATUS __ami_rpc_decode_rc2_params(uchar_t * encodedData,
    size_t length,
    ami_octetstring **pIv,
    int *effectiveKeySize);

/*
 * RPC for AMISERV_AUX
 */
AMI_STATUS __ami_rpc_get_amiserv_aux_client_handle(CLIENT **clnt);

AMI_STATUS __ami_rpc_encode_printable_string(ami_printable_string printableString,
    uchar_t **paramValue, size_t *paramLength);

AMI_STATUS __ami_rpc_decode_printable_string(uchar_t * encodedData,
    size_t length, ami_printable_string * pData);

AMI_STATUS __ami_rpc_encode_IA5_string(ami_email_addr str,
    uchar_t **paramValue, size_t *paramLength);

AMI_STATUS __ami_rpc_decode_IA5_string(uchar_t *encodedData,
    size_t length, ami_email_addr *pData);

AMI_STATUS __ami_rpc_encode_octet_string(ami_octetstring *iv,
    uchar_t **paramValue, size_t *paramLength);

AMI_STATUS _ami_rpc_decode_octet_string(uchar_t * encodedData,
    size_t length, ami_octetstring **pIv);

AMI_STATUS __ami_rpc_encode_rc2_params(ami_octetstring *iv,
    int effectiveKeySize, uchar_t **paramValue, size_t *paramLength);

AMI_STATUS __ami_rpv_decode_rc2_params(uchar_t * encodedData,
    size_t length, ami_octetstring **pIv, int *effectiveKeySize);

AMI_STATUS __ami_rpc_decode_certificate(const void *binary_cert,
    int binary_cert_len, ami_cert *ppCert);

AMI_STATUS __ami_rpc_encode_certificate(const ami_cert_info *pCert,
    uchar_t **paramValue, size_t *paramLength);

/*
 * Crypto
 */
AMI_STATUS __ami_get_rsa_pubkey_components PROTO_LIST((
	uchar_t *, u_int, uchar_t **, u_int *, uchar_t **, u_int *));

/*
 * DH
 */
AMI_STATUS __ami_dh_part1 PROTO_LIST((
	ami_handle *,
	unsigned int,		/* size of random exponent in bits */
	unsigned int,		/* size of prime modulus in bytes */
	unsigned char *,	/* prime modulus */
	unsigned int,		/* size of base generator in bytes */
	unsigned char *,	/* base generator */
	unsigned char *,	/* seed for random number generator */
	unsigned int,		/* size of seed in bytes */
	unsigned char *,	/* output buufer for public key */
	unsigned int *,		/* ptr to output length holder */
	unsigned int));		/* max length of output buffer */

AMI_STATUS __ami_dh_part2 PROTO_LIST((
	ami_handle *,
	unsigned char *,
	unsigned int *,
	unsigned int,
	unsigned char *,
	unsigned int));

/*
 * Misc.
 */
AMI_STATUS __ami_append_cert_to_chain PROTO_LIST((
	ami_cert *,
        ami_cert *,
	int,
	 ami_cert **,
	int *));

#ifdef	__cplusplus
}
#endif

#endif	/* _AMI_PROTO_H */
