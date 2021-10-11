#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)ami.spec	1.4 99/07/23 SMI"
#
# lib/ami/spec/ami.spec

function	ami_init
include		<security/ami.h>
declaration	AMI_STATUS ami_init(const char *appName, \
		const char *backend, const char *alias, \
		const char *hostname, uint_t flags, \
		uint_t crypto_define, const char *ldd, \
		ami_handle_t *amih)
version		SUNW_1.1
exception	$return != 0
end		

function	ami_end
include		<security/ami.h>
declaration	AMI_STATUS ami_end(ami_handle_t amih)
version		SUNW_1.1
exception	$return != 0
end		

function	ami_strerror
include		<security/ami.h>
declaration	char *ami_strerror(ami_handle_t amih, \
		const AMI_STATUS errorno)
version		SUNW_1.1
exception	$return != 0
end		

function	ami_gen_symmetric_key
include		<security/ami.h>
declaration	AMI_STATUS ami_gen_symmetric_key(const ami_handle_t amih, \
		ami_mechanism, uint_t, uint_t, ami_session_key_t *, \
		ami_session_key_params_t *)
version		SUNW_1.1
exception	$return != 0
end		

function	ami_digest
include		<security/ami.h>
declaration	AMI_STATUS ami_digest(ami_handle_t amih, const uchar_t *data, \
		size_t length, int flag, ami_mechanism, \
		uchar_t **digest, size_t *digestLen)
version		SUNW_1.1
exception	$return != 0
end		

function	ami_sign
include		<security/ami.h>
declaration	AMI_STATUS ami_sign(ami_handle_t amih, const uchar_t *data, \
		size_t length, int flag, ami_mechanism, \
		const ami_private_key_t, \
		ami_mechanism, uchar_t **signature, \
		size_t *signlenght)
version		SUNW_1.1
exception	$return != 0
end		

function	ami_verify
include		<security/ami.h>
declaration	AMI_STATUS ami_verify(ami_handle_t amih, const uchar_t *data, \
		size_t length, int flag, ami_mechanism, \
		const ami_public_key_t, \
		ami_mechanism, const uchar_t *sign, \
		const size_t signlen)
version		SUNW_1.1
exception	$return != 0
end		

function	ami_encrypt
include		<security/ami.h>
declaration	AMI_STATUS ami_encrypt(ami_handle_t amih, const uchar_t *data, \
		size_t length, int flag, ami_mechanism, \
		const ami_session_key_t, const ami_session_key_params_t, \
		uchar_t **cipher, size_t *cipherlen)
version		SUNW_1.1
exception	$return != 0
end

function	ami_decrypt
include		<security/ami.h>
declaration	AMI_STATUS ami_decrypt(ami_handle_t amih, const uchar_t *data, \
		size_t len, int flag, ami_mechanism, \
		const ami_session_key_t, const ami_session_key_params_t, \
		uchar_t **text,	size_t *textlength)
version		SUNW_1.1
exception	$return != 0
end		

function	ami_wrap_key
include		<security/ami.h>
declaration	AMI_STATUS ami_wrap_key(const ami_handle_t amih, \
		const ami_session_key_t, ami_mechanism, \
		const ami_public_key_t, ami_mechanism, \
		uchar_t **wraped, size_t *wrapedlen)
version		SUNW_1.1
exception	$return != 0
end		

function	ami_unwrap_key
include		<security/ami.h>
declaration	AMI_STATUS ami_unwrap_key(const ami_handle_t amih, \
		const uchar_t *uwkey, const size_t len, \
		ami_mechanism, const ami_private_key_t, \
		ami_mechanism, ami_session_key_t *)
version		SUNW_1.1
exception	$return != 0
end		

function	ami_verify_cert
include		<security/ami.h>
declaration	AMI_STATUS ami_verify_cert(const ami_handle_t amih, \
		const ami_cert_t cert, const ami_public_key_t publickey)
version		SUNW_1.1
exception	$return != 0
end		

function	ami_get_cert
include		<security/ami.h>
declaration	AMI_STATUS ami_get_cert(const ami_handle_t amih, \
		const char *name, ami_cert_list_t *certs)
version		SUNW_1.1
exception	$return != 0
end		

function	ami_get_trusted_cert_list
include		<security/ami.h>
declaration	AMI_STATUS ami_get_trusted_cert_list(const ami_handle_t amih, \
		ami_cert_list_t *certs)
version		SUNW_1.1
exception	$return != 0
end		

function	ami_get_private_key
include		<security/ami.h>
declaration	AMI_STATUS ami_get_private_key(const ami_handle_t amih, \
		ami_private_key_t *keypkg)
version		SUNW_1.1
exception	$return != 0
end		

function	ami_get_cert_first
include		<security/ami.h>
declaration	AMI_STATUS ami_get_cert_first(ami_handle_t amih, \
		ami_cert_list_t certlist, ami_cert_t *cert)
version		SUNW_1.1
exception	$return != 0
end

function	ami_get_cert_next
include		<security/ami.h>
declaration	AMI_STATUS ami_get_cert_next(ami_handle_t amih, \
		ami_cert_list_t certlist, ami_cert_t *cert)
version		SUNW_1.1
exception	$return != 0
end

function	ami_get_public_key_mechanism
include		<security/ami.h>
declaration	ami_mechanism ami_get_public_key_mechanism( \
		ami_public_key_t certlist)
version		SUNW_1.1
exception	$return != 0
end

function	ami_get_private_key_mechanism
include		<security/ami.h>
declaration	ami_mechanism ami_get_private_key_mechanism( \
		ami_private_key_t certlist)
version		SUNW_1.1
exception	$return != 0
end

function	ami_get_public_key_from_cert
include		<security/ami.h>
declaration	ami_public_key_t ami_get_public_key_from_cert( \
		ami_cert_t cert)
version		SUNW_1.1
exception	$return == 0
end

function	ami_get_cert_valid_from
include		<security/ami.h>, <time.h>
declaration	struct tm *ami_get_cert_valid_from(ami_cert_t cert)
version		SUNW_1.1
exception	$return == 0
end

function	ami_get_cert_valid_to
include		<security/ami.h>
declaration	struct tm *ami_get_cert_valid_to(ami_cert_t cert)
version		SUNW_1.1
exception	$return == 0
end

function	ami_get_cert_signature
include		<security/ami.h>
declaration	AMI_STATUS ami_get_cert_signature(ami_cert_t cert, \
		uchar_t **sign, size_t *signLen)
version		SUNW_1.1
exception	$return != 0
end

function	ami_get_cert_subject
include		<security/ami.h>
declaration	char *ami_get_cert_subject(ami_cert_t cert)
version		SUNW_1.1
exception	$return == 0
end

function	ami_get_cert_issuer
include		<security/ami.h>
declaration	char *ami_get_cert_issuer(ami_cert_t cert)
version		SUNW_1.1
exception	$return == 0
end

function	ami_get_cert_serial_number
include		<security/ami.h>
declaration	AMI_STATUS ami_get_cert_serial_number(ami_cert_t cert, \
		uchar_t **sNumber, size_t *lenght)
version		SUNW_1.1
exception	$return == 0
end

function	ami_free_symmetric_key
include		<security/ami.h>
declaration	void ami_free_symmetric_key(ami_session_key_t keypkg)
version		SUNW_1.1
end		

function	ami_free_symmetric_key_params
include		<security/ami.h>
declaration	void ami_free_symmetric_key_params( \
		ami_session_key_params_t keyparams)
version		SUNW_1.1
end		

function	ami_free_private_key
include		<security/ami.h>
declaration	void ami_free_private_key(ami_private_key_t keypkg)
version		SUNW_1.1
end		

function	ami_free_public_key
include		<security/ami.h>
declaration	void ami_free_public_key(ami_public_key_t pubkey)
version		SUNW_1.1
end		

function	ami_free_cert
include		<security/ami.h>
declaration	void ami_free_cert(ami_cert_t cert)
version		SUNW_1.1
end		

function	ami_free_cert_list
include		<security/ami.h>
declaration	void ami_free_cert_list(ami_cert_list_t cert)
version		SUNW_1.1
end		

function	ami_get_cert_chain
version		SUNWprivate_1.1
end

function	ami_verify_cert_chain
version		SUNWprivate_1.1
end

function	ami_keymgnt_get_default_rsa_signature_key_alias
version		SUNWprivate_1.1
end

function	ami_keymgnt_get_default_rsa_encryption_key_alias
version		SUNWprivate_1.1
end

function	ami_keymgnt_get_default_dsa_key_alias
version		SUNWprivate_1.1
end

function	ami_keymgnt_get_default_dh_key_alias
version		SUNWprivate_1.1
end

function	ami_keymgnt_get_keystore
version		SUNWprivate_1.1
end

function	ami_keymgnt_set_keystore
version		SUNWprivate_1.1
end

function	ami_keymgnt_get_certificates
version		SUNWprivate_1.1
end

function	__ami_rpc_set_keystore
version		SUNWprivate_1.1
end

function	__ami_rpc_change_keystore_password
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_keygen_AMI_1KeyGen_ami_1extract_1public_1modexp
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_keygen_AMI_1KeyGen_ami_1extract_1private_1modexp
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_crypto_AMI_1Crypto_ami_1end
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_crypto_AMI_1Crypto_ami_1init
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_crypto_AMI_1Crypto_ami_1rc2_1decrypt
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_crypto_AMI_1Crypto_ami_1rc2_1encrypt
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_crypto_AMI_1Crypto_ami_1rc4_1decrypt
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_crypto_AMI_1Crypto_ami_1rc4_1encrypt
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_crypto_AMI_1Crypto_ami_1des3des_1encrypt
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_crypto_AMI_1Crypto_ami_1des3des_1decrypt
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_crypto_AMI_1Crypto_ami_1rsa_1unwrap
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_crypto_AMI_1Crypto_ami_1rsa_1wrap
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_digest_AMI_1Digest_ami_1md2_1digest
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_digest_AMI_1Digest_ami_1md5_1digest
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_digest_AMI_1Digest_ami_1sha1_1digest
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_keygen_AMI_1KeyGen_ami_1gen_1rsa_1keypair
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_keygen_AMI_1KeyGen_ami_1gen_1des3des_1key 	
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_sign_AMI_1Signature_ami_1rsa_1sign
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_sign_AMI_1Signature_ami_1rsa_1verify
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FNS_1Enumeration_fns_1list_1next
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FNS_fns_1add_1attribute
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FNS_fns_1add_1binary_1attribute
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FNS_fns_1delete_1attribute
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FNS_fns_1delete_1binary_1attribute
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FNS_fns_1destroy_1context
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FNS_fns_1get_1attribute
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FNS_fns_1get_1attributeIDs
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FNS_fns_1search
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FNS_fns_1set_1naming_1context
version		SUNWprivate_1.1
end

function 	Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FNS_fns_1context_1handle_1destroy
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FILE_ami_1get_1user_1home_1directory
version		SUNWprivate_1.1
end

function 	Java_com_sun_ami_utils_AMI_1C_1Utils_ami_1get_1password
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_utils_AMI_1C_1Utils_ami_1get_1user_1id
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_amiserv_AMI_1KeyServClient_1RPC_initRPC
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_amiserv_AMI_1KeyServClient_1RPC_signDataRPC
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_amiserv_AMI_1KeyServClient_1RPC_setKeyStoreRPC
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_amiserv_AMI_1KeyServClient_1RPC_unwrapDataRPC
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_amiserv_AMI_1KeyServClient_1RPC_getKeyStoreRPC
version		SUNWprivate_1.1
end

function	Java_com_sun_ami_keymgnt_AMI_1KeyMgnt_1FNS_fns_1get_1euid
version		SUNWprivate_1.1
end
