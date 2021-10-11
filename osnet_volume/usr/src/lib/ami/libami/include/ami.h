/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_AMI_H
#define	_AMI_H

#pragma ident	"@(#)ami.h	1.6	99/07/23 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <time.h>
#include <sys/types.h>

/*
 * AMI supported crypto mechanisms
 */
typedef enum ami_mechanism {
	AMI_RSA = 1,		/* RSA key */
	AMI_DSA,		/* DSA key */
	AMI_MD2,		/* MD2 message digest */
	AMI_MD5,		/* MD5 message digest */
	AMI_SHA1,		/* SHA1 message digest */
	AMI_RSA_ENCR,		/* RSA Encryption algorithm */
	AMI_MD2WithRSAEncryption,	/* MD2 with RSA encryption */
					/* signature algorithm */
	AMI_MD5WithRSAEncryption,	/* MD5 with RSA encryption */
					/* signature algorithm */
	AMI_SHA1WithRSAEncryption,	/* SHA1 with RSA encryption */
					/* signature algorithm */
	AMI_SHA1WithDSASignature, /* SHA1 with DSA signature algorithm */
	AMI_DES_CBC,		/* DES encryption algorithm */
	AMI_DES3_CBC,		/* triple DES encryption algorithm */
	AMI_RC2_CBC,		/* RC2 encryption algorithm */
	AMI_RC4			/* RC4 encryption algorithm */
} ami_mechanism;


/*
 * AMI function return values
 */
#define	AMI_OK				0
#define	AMI_EBUFSIZE			1
#define	AMI_ENOMEM			2
#define	AMI_BAD_FILE			3
#define	AMI_FILE_NOT_FOUND		4
#define	AMI_FILE_IO_ERR			5
#define	AMI_BAD_PASSWD			6
#define	AMI_UNKNOWN_USER		7
#define	AMI_ALGORITHM_UNKNOWN		8
#define	AMI_ASN1_ENCODE_ERR		9
#define	AMI_ASN1_DECODE_ERR		10
#define	AMI_BAD_KEY			11
#define	AMI_KEYGEN_ERR			12
#define	AMI_ENCRYPT_ERR			13
#define	AMI_DECRYPT_ERR			14
#define	AMI_SIGN_ERR			15
#define	AMI_VERIFY_ERR			16
#define	AMI_DIGEST_ERR			17
#define	AMI_OUTPUT_FORMAT_ERR		18
#define	AMI_SYSTEM_ERR			19
#define	AMI_ATTRIBUTE_UNKNOWN		20
#define	AMI_AMILOGIN_ERR		21
#define	AMI_AMILOGOUT_ERR		22
#define	AMI_NO_SUCH_ENTRY		23
#define	AMI_ENTRY_ALREADY_EXISTS	24
#define	AMI_AMISERV_DECRYPT_ERR		25
#define	AMI_AMISERV_SIGN_ERR		26
#define	AMI_USER_DID_NOT_AMILOGIN	27
#define	AMI_AMISERV_CONNECT		28
#define	AMI_KEYPKG_NOT_FOUND		29
#define	AMI_TIME_INVALID		30
#define	AMI_UNTRUSTED_PUBLIC_KEY	31
#define	AMI_EPARM			32
#define	AMI_BINARY_TO_RFC1421_ERR	33
#define	AMI_RFC1421_TO_BINARY_ERR	34
#define	AMI_RANDOM_NUM_ERR		35
#define	AMI_XFN_ERR			36
#define	AMI_CERT_CHAIN_ERR		37
#define	AMI_RDN_MISSING_EQUAL		38
#define	AMI_AVA_TYPE_MISSING		39
#define	AMI_AVA_VALUE_MISSING		40
#define	AMI_CERT_NOT_FOUND		41
#define	AMI_DN_NOT_FOUND		42
#define	AMI_CRITICAL_EXTNS_ERR		43
#define	AMI_ASN1_INIT_ERROR		44
#define	AMI_WRAP_ERROR			45
#define	AMI_UNWRAP_ERROR		46
#define	AMI_UNSUPPORTED_KEY_TYPE	47
#define	AMI_DH_PART1_ERR		48
#define	AMI_DH_PART2_ERR		49
#define	AMI_DOUBLE_ENCRYPT		50
#define	AMI_AMISERV_KEYPKG_UPDATE	51
#define	AMI_AMISERV_STAT_ERR		52
#define	AMI_GLOBAL_ERR			53
#define	AMI_TRUSTED_KEY_EXPIRED		54
#define	AMI_OPEN_ERR			55
#define	AMI_TOTAL_ERRNUM		56
#define	AMI_CERT_ERR			57
#define	AMI_KEYPKG_ERR			58

/* flags for ami_encrypt, ami_decrypt, ami_sign, ami_verify & ami_digest */
#define	AMI_ADD_DATA	1
#define	AMI_END_DATA	2
#define	AMI_DIGESTED_DATA 3 /* for ami_verify only */
#define	AMI_DIGESTED_AND_ENCODED_DATA 4 /* for ami_verify only */

/* AMI return variable */
typedef uint AMI_STATUS;

/* AMI Handle and status */
typedef struct ami_handle *ami_handle_t;

/* Session keys and parameters */
typedef struct ami_session_key *ami_session_key_t;
typedef struct Any *ami_session_key_params_t;

/* Public keys, private keys and certificates */
typedef struct ami_private_key *ami_private_key_t;
typedef struct ami_public_key *ami_public_key_t;
typedef struct ami_cert *ami_cert_t;
typedef struct ami_nss_cert_list *ami_cert_list_t;

/*
 * PROTOTYPES should be set to one if and only if the compiler supports
 * function argument prototyping.
 * The following makes PROTOTYPES default to 1 if it has not already been
 * defined as 0 with C compiler flags.
 */
#ifndef	PROTOTYPES
#define	PROTOTYPES	1
#endif

/*
 * PROTO_LIST is defined depending on how PROTOTYPES is defined above.
 * If using PROTOTYPES, then PROTO_LIST returns the list, otherwise it
 * returns an empty list.
 */

#if PROTOTYPES
#define	PROTO_LIST(list) list
#else
#define	PROTO_LIST(list) ()
#endif

/*
 * AMI prototypes
 */

/* Init a AMI session */
AMI_STATUS ami_init PROTO_LIST((
    const char *appName,    /* Application name */
    const char *backend,    /* Backend data store */
    const char *alias,	    /* Alias of the private key */
    const char *hostname,   /* Support for virtual hosting */
    uint_t flags,	    /* Unused */
    uint_t crypto_define,   /* Unused */
    const char *ldd,	    /* Unused */
    ami_handle_t *amih	    /* OUT: Handle to AMI session */
));

/* Terminate the AMI session */
AMI_STATUS ami_end PROTO_LIST((
	ami_handle_t 		/* IN: AMI handle */
));

/* internationalized error message */
char *ami_strerror PROTO_LIST((
	ami_handle_t,		/* IN: AMI handle */
	const AMI_STATUS	/* IN: errno */
));

/* Symmetric key generation */
AMI_STATUS ami_gen_symmetric_key PROTO_LIST((
    const ami_handle_t,	/* IN: AMI handle */
    ami_mechanism,		/* IN: Secret key type */
    uint_t,			/* IN: key length */
    uint_t,			/* IN: effective key size */
    ami_session_key_t *,	/* OUT: session key */
    ami_session_key_params_t *	/* OUT: parameters */
));

/* crypto */
AMI_STATUS ami_digest PROTO_LIST((
	ami_handle_t,			/* IN:	ami handle */
	const uchar_t *,		/* IN:  input data  */
	size_t,				/* IN:  length of data in bytes */
	int,				/* IN:  more input data flag */
	ami_mechanism,			/* IN:  digest algorithm */
	uchar_t **,			/* OUT: digest */
	size_t *));			/* OUT: length of digest */

AMI_STATUS ami_sign PROTO_LIST((
	ami_handle_t,			/* IN:	ami handle */
	const uchar_t *,		/* IN:  data to be signed */
	size_t,				/* IN:  data length */
	int,				/* IN:  more input data flag */
	ami_mechanism,			/* IN:  signature key algorithm */
	const ami_private_key_t,	/* IN:  signature key */
	ami_mechanism,			/* IN:  signature algorithm */
	uchar_t **, 			/* OUT: signature */
	size_t *));			/* OUT: signature length */

AMI_STATUS ami_verify PROTO_LIST((
	ami_handle_t,			/* IN: ami handle */
	const uchar_t *, 		/* IN: data to be verified */
	size_t,				/* IN: data length */
	int,				/* IN: more input data flag */
	ami_mechanism,			/* IN: verification key algorithm */
	const ami_public_key_t,		/* IN: verification key */
	ami_mechanism,			/* IN: verification algorithm */
	const uchar_t *, 		/* IN: signature */
	const size_t));			/* IN: signature length */

AMI_STATUS ami_encrypt PROTO_LIST((
	ami_handle_t,			/* IN:	ami handle */
	const uchar_t *,		/* IN:  input data */
	size_t,				/* IN:  input data length */
	int,				/* IN:	more input data flag */
	ami_mechanism,			/* IN:  encryption algorithm */
	const ami_session_key_t,	/* IN:  encryption key */
	const ami_session_key_params_t,	/* IN:  key params */
	uchar_t **,			/* OUT: ciphertext */
	size_t *));			/* OUT: ciphertext length */

AMI_STATUS ami_decrypt PROTO_LIST((
	ami_handle_t,			/* IN:	ami handle */
	const uchar_t *,		/* IN:  ciphertext */
	size_t,				/* IN:  ciphertext length */
	int,				/* IN:  more input data flag */
	ami_mechanism,			/* IN:  decryption key algorithm */
	const ami_session_key_t,	/* IN:  decryption key */
	const ami_session_key_params_t,	/* IN:  key params */
	uchar_t **,			/* OUT: cleartext */
	size_t *));			/* OUT: cleartext length */

AMI_STATUS ami_wrap_key PROTO_LIST((
	const ami_handle_t,		/* IN:  ami handle */
	const ami_session_key_t,	/* IN:	key to be wrapped  */
	ami_mechanism,			/* IN:	wrapping key algorithm */
	const ami_public_key_t,		/* IN:	wrapping key */
	ami_mechanism,			/* IN:	wrapping algorithm */
	uchar_t **,			/* OUT: wrapped key */
	size_t *));			/* OUT: wrapped key length */

AMI_STATUS ami_unwrap_key PROTO_LIST((
	const ami_handle_t,		/* IN:  ami handle */
	const uchar_t *,		/* IN:  wrapped key */
	size_t,				/* IN:  wrapped key length */
	ami_mechanism,			/* IN:  unwrapping key algorithm */
	const ami_private_key_t,	/* IN:  unwrapping key */
	ami_mechanism,			/* IN:  unwrapping algorithm */
	ami_session_key_t *));		/* OUT: unwrapped key */

/* Function to get private key object */
AMI_STATUS ami_get_private_key PROTO_LIST((
	const ami_handle_t,	/* IN:	ami handle */
	ami_private_key_t *));	/* OUT: private key */

/* Function to get public key object */
AMI_STATUS ami_get_cert PROTO_LIST((
	const ami_handle_t,	/* IN:	ami handle */
	const char *,		/* IN:  certificate filename, rep index, DN */
	ami_cert_list_t *));	/* OUT: set of certificates */

/* Fuction to get the trusted certificates of a user */
AMI_STATUS
ami_get_trusted_cert_list PROTO_LIST((
	ami_handle_t amih,	/* IN:	ami handle */
	ami_cert_list_t *certlist /* OUT: list of trusted certificates */
));

/* certificate chain establishment */
AMI_STATUS ami_get_cert_chain PROTO_LIST((
	const ami_handle_t,	/* IN: ami handle */
	const ami_cert_t,	/* IN: user certificate */
	const char **,		/* IN: CA name list */
	ami_cert_list_t *));	/* OUT: certificate chain */

/* certificate verification */
AMI_STATUS ami_verify_cert PROTO_LIST((
	const ami_handle_t,		/* IN: ami handle */
	const ami_cert_t, 		/* IN: certificate to be verified */
	const ami_public_key_t));	/* IN: public verification key */

/* generate random bytes */
AMI_STATUS ami_random PROTO_LIST((
	const ushort_t,		/* IN:  requested number of random bytes */
	uchar_t **));		/* OUT: random byte buffer */

/* certificate and certificate list operations */
AMI_STATUS ami_get_cert_first PROTO_LIST((
    ami_handle_t amih,		/* AMI Handle */
    ami_cert_list_t certlist,	/* Certificate list object */
    ami_cert_t *cert		/* OUT: first certificate */
));

AMI_STATUS ami_get_cert_next PROTO_LIST((
    ami_handle_t amih,		/* AMI Handle */
    ami_cert_list_t certlist,	/* Certificate list object */
    ami_cert_t *cert		/* OUT: next certificate */
));


/* Functions to obtain individual components from certificate */
/* Public key object */
ami_public_key_t ami_get_public_key_from_cert PROTO_LIST((
    ami_cert_t cert));

/* Certificate valid from */
struct tm *ami_get_cert_valid_from PROTO_LIST((
    ami_cert_t cert));

/* Certificate valid to */
struct tm *ami_get_cert_valid_to PROTO_LIST((
    ami_cert_t cert));

/* Signature of the certificate */
AMI_STATUS ami_get_cert_signature PROTO_LIST((ami_cert_t cert,
    uchar_t **sign, size_t *signLen));

/* Get subject name */
char *ami_get_cert_subject PROTO_LIST((ami_cert_t cert));

/* Get issuer name */
char *ami_get_cert_issuer PROTO_LIST((ami_cert_t cert));

/* Get the serial number of the certificate */
AMI_STATUS ami_get_cert_serial_number PROTO_LIST((ami_cert_t cert,
    uchar_t **serialNumber, size_t *length));

/* Functions for public and private key operations */
/* Get public key mechanism */
ami_mechanism ami_get_public_key_mechanism PROTO_LIST((
    ami_public_key_t publicKey));

/* function to obtain the key mechanism */
ami_mechanism ami_get_private_key_mechanism PROTO_LIST((
    ami_private_key_t privateKey));

/* Free */
void ami_free_symmetric_key PROTO_LIST((
	ami_session_key_t));

void ami_free_symmetric_key_params PROTO_LIST((
	ami_session_key_params_t));

void ami_free_private_key PROTO_LIST((
	ami_private_key_t));

void ami_free_public_key PROTO_LIST((
	ami_public_key_t));

void ami_free_cert PROTO_LIST((
	ami_cert_t));

void ami_free_cert_list PROTO_LIST((
	ami_cert_list_t));

#ifdef	__cplusplus
}
#endif

#endif	/* _AMI_H */
