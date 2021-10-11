/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_AMI_LOCAL_H
#define	_AMI_LOCAL_H

#pragma ident "@(#)ami_local.h	1.4 99/12/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <time.h>
#include <stdio.h>
#include <nl_types.h>
#include <string.h>

/*
 * Some defined values
 */
#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#define	AMI_MD2_DIGEST_LEN 16
#define	AMI_MD5_DIGEST_LEN 16
#define	AMI_SHA1_DIGEST_LEN 20
#define	AMI_LINE_LEN	512	/* username and DN must be less than this */

#define	AMI_ESCAPE_COMMA	1
#define	AMI_NOT_ESCAPE_COMMA	2

#define	AMI_NORMAL_VERBOSE	1
#define	AMI_TRULY_VERBOSE	2

#define	AMI_LINEWRAP    	64
#define	AMI_SALT_LEN 		8
#define	AMI_ITERATION_COUNT 	10	/*
					 * iteration count for DES3 key
					 * generation, must be greater
					 * than 2
					 */
#define	AMI_NSA_DES_ZERO_MASK	0x0F
#define	AMI_GLOBAL_NSA_DES_ZERO_MASK	0x0F
#define	AMI_GLOBAL_NSA_DES_ZERO_INVERSE_MASK	0xF0
#define	AMI_NSA_DES_ZERO_INVERSE_MASK	0xF0
#define	AMI_DES_KEY_LEN		8
#define	AMI_DES_EDE_KEY_LEN 	24

/* key size (in bytes) of symmetric encryption keys (export) */
#define	AMI_WEAK_KEY_SIZE	5
#define	AMI_GLOBAL_KEY_SIZE	5
#define AMI_DOMESTIC_KEY_SIZE   16

#define AMI_RC2_WEAK_EFFECTIVE_KEYSIZE 40
#define AMI_RC2_EXPORT_EFFECTIVE_KEYSIZE 40
#define AMI_RC2_DOMESTIC_EFFECTIVE_KEYSIZE 128
#define	AMI_WEAK_RSA_KEY_SIZE	512
#define	AMI_GLOBAL_RSA_KEY_SIZE	512
#define	AMI_DOMESTIC_RSA_KEY_SIZE	2048

#ifdef AMI_DOMESTIC
#define maxAsymmetricKeySize AMI_DOMESTIC_RSA_KEY_SIZE
#define maxSymmetricKeySize AMI_DOMESTIC_KEY_SIZE
#define maxRC2effectiveKeySize AMI_RC2_DOMESTIC_EFFECTIVE_KEYSIZE
#endif

#ifdef AMI_GLOBAL
#define maxAsymmetricKeySize AMI_GLOBAL_RSA_KEY_SIZE
#define maxSymmetricKeySize AMI_GLOBAL_KEY_SIZE
#define maxRC2effectiveKeySize AMI_RC2_EXPORT_EFFECTIVE_KEYSIZE
#endif

#ifdef AMI_WEAK
#define maxAsymmetricKeySize AMI_WEAK_RSA_KEY_SIZE
#define maxSymmetricKeySize AMI_WEAK_KEY_SIZE
#define maxRC2effectiveKeySize AMI_RC2_WEAK_EFFECTIVE_KEYSIZE
#endif

#define	AMI_2ENCRYPT_BUF_SIZE	64
#define	AMI_NSA_LEEWAY		64

#define	AMI_IV_LEN 		8
#define	AMI_VRSN_1_0 		"1.0"
#define	AMI_MAX_PASSPHRASE_LEN 	100
#define	AMI_NEW_PHRASE 			1
#define	AMI_OLD_PHRASE 			2
#define	AMI_AMILOGIN_USER_PHRASE	3
#define	AMI_CREATE_CREDS_PHRASE		4
#define	AMI_AMILOGIN_HOST_PHRASE	5
#define	AMI_MAX_PROMPT 		3 /* max number of passphrase prompt cycles */

/* bufsizes for multiple-part operations */
#define	EXTRA_UPDATE_BYTES	24
#define	UPDATE_OUTPUT_SIZE (BUFSIZ + EXTRA_UPDATE_BYTES)
	/* see Bsafe 2.1 User's Manual, p.31 */

/* Time flags */
#define	AMI_NX_DAY		1

/* AMI file delimiter (between certificates and keypkgs) */
#define	AMI_FILE_DELIMITER	'#'
#define	AMI_DEFAULT_KEYID	"mykey"
#define	AMI_DEFAULT_HOST_IP	"0.0.0.0"

/* flags for __ami_get_rsa_keysize() */
#define	AMI_RSA_PUBLIC	1
#define	AMI_RSA_PRIVATE	2

/* DN separators */
#define	AMI_RDN_SEPARATOR_CHR	','
#define	AMI_RDN_SEPARATOR_STR	","
#define	AMI_AVA_SEPARATOR_CHR	'+'
#define	AMI_AVA_SEPARATOR_STR	"+"

typedef enum ami_data_format {
	AMI_BINARY = 1,
	AMI_RFC1421 = 2,
	AMI_HEX = 3
} ami_data_format;

/*
 * Type definitions
 */
typedef u_int RFC1421_SESSION_HANDLE;
typedef u_int rsa_parm_type;

/* OID structure */
typedef struct ami_oid {
    unsigned short  count;
    unsigned long   *value;
} ami_oid;

/* Attributr table */
typedef struct AttrList {
	ami_oid	*oid;
	char *name;
} AttrList, *AttrList_PTR;

typedef struct Any {
    unsigned long   length;
    unsigned char   *value;
} Any;

typedef struct ami_rdn_seq *ami_dname;

typedef struct ami_name {
    unsigned short choice;
#define	distinguishedName_chosen 1
	union {
		struct ami_rdn_seq *distinguishedName;
	} u;
} ami_name;

typedef struct ami_rdn_seq {
    struct ami_rdn_seq *next;
    struct ami_rdname *value;
} *ami_rdn_seq;

typedef struct ami_rdname {
    struct ami_rdname *next;
    struct ami_ava  *value;
} *ami_rdname;

typedef Any ami_attr_value;

typedef struct ami_ava {
    struct ami_oid *objid;
    ami_attr_value  *value;
} ami_ava;

typedef struct ami_attr_list {
    struct ami_attr_list *next;
    struct ami_attr *value;
} *ami_attr_list;

typedef struct ami_attr {
    struct ami_oid *type;
    struct ami_attr_value_set *values;
} ami_attr;

typedef struct ami_attr_value_set {
    struct ami_attr_value_set *next;
    ami_attr_value  *value;
} *ami_attr_value_set;

typedef struct CaseIgnoreString {
    unsigned short choice;
#define	CaseIgnoreString_t61String_chosen 1
#define	CaseIgnoreString_printableString_chosen 2
	union {
		char *CaseIgnoreString_t61String;
		char *CaseIgnoreString_printableString;
	} u;
} CaseIgnoreString;

typedef CaseIgnoreString ami_case_ignore_string;

typedef char *ami_printable_string;

typedef struct ami_cert_pair {
    struct ami_cert *forward;  /* NULL for not present */
    struct ami_cert *reverse;  /* NULL for not present */
} ami_cert_pair;

typedef struct ami_cert_serialnum {
    unsigned short  length;
    unsigned char   *value;
} ami_cert_serialnum;

typedef struct ami_cert_info {
    unsigned char bit_mask;
#define	version_present 0x80
#define	extensions_present 0x40
    int version; /* default assumed if omitted */
#define	version_v1 0
#define	version_v2 1
#define	version_v3 2
    ami_cert_serialnum serial;
    struct ami_algid *signature;
    struct ami_name *issuer;
    struct ami_validity *validity;
    struct ami_name *subject;
    struct ami_pubkey_info *pubKeyInfo;
    struct ami_uid  *issuerUID;  /* NULL for not present */
    struct ami_uid  *subjectUID;  /* NULL for not present */
    struct ami_cert_extn_list *extensions;  /* optional */
} ami_cert_info;

typedef struct ami_bitstring {
    unsigned int    length;  /* number of significant bits */
    unsigned char   *value;
} ami_bitstring;

typedef struct ami_cert {
    ami_cert_info   info;
    struct ami_algid *algorithm;
    ami_bitstring   signature;
} ami_cert;

typedef struct ami_uid {
    unsigned int    length;  /* number of significant bits */
    unsigned char   *value;
} ami_uid;

typedef struct ami_octetstring {
    unsigned int    length;
    unsigned char   *value;
} ami_octetstring;

typedef int ami_cert_version;
#define	CertificateVersion_v1 0
#define	CertificateVersion_v2 1
#define	CertificateVersion_v3 2

typedef char amiBoolean;

typedef struct {
    short year; /* YYYY format when used for GeneralizedTime */
			/* YY format when used for UTCTime */
    short month;
    short day;
    short hour;
    short minute;
    short second;
    short millisec;
    short mindiff;  /* UTC +/- minute differential */
    amiBoolean utc; /* TRUE means UTC time */
} GeneralizedTime;

typedef GeneralizedTime UTCTime;

typedef struct ami_validity {
    UTCTime *notBefore;
    UTCTime *notAfter;
} ami_validity;

typedef struct ami_pubkey_info {
    struct ami_algid *algorithm;
    ami_bitstring   pubKey;
} ami_pubkey_info;

typedef Any ami_alg_params;

typedef struct ami_algid {
    struct ami_oid *algorithm;
    ami_alg_params *parameters;  /* NULL for not present */
} ami_algid;

typedef struct ami_cert_extn {
    unsigned char bit_mask;
#define	critical_present 0x80
    struct ami_oid *extend;
    amiBoolean critical;
    ami_octetstring extnValue;
} ami_cert_extn;

typedef struct ami_cert_extn_list {
    struct ami_cert_extn_list *next;
    struct ami_cert_extn *value;
} *ami_cert_extn_list;

typedef struct ami_cert_list_contents {
    unsigned char bit_mask;
#define	nextUpdate_present 0x80
#define	CertListContents_revokedCertificates_present 0x40
    ami_algid signature;
    ami_name issuer;
    UTCTime thisUpdate;
    UTCTime nextUpdate;
	struct _seqof1 {
		struct _seqof1  *next;
		struct {
			ami_cert_serialnum userCertificate;
			UTCTime revocationDate;
		} value;
	} *CertListContents_revokedCertificates;
} ami_cert_list_contents;

typedef struct ami_cert_list {
    ami_cert_list_contents certListContents;
    ami_algid algId;
    ami_bitstring signature;
} ami_cert_list;

typedef struct ami_rc2_cbc_param {
    unsigned short choice;
#define	 iv_chosen 1
#define	sequence_chosen 2
	union {
		ami_octetstring iv;
		struct _seq1 {
			int version;
			ami_octetstring iv;
		} sequence;
	} u;
} ami_rc2_cbc_param;

typedef int INT;

typedef struct ami_keypkg_info {
    unsigned char bit_mask;
#define	keypkgAttrs_present 0x80
#define	tKeys_present 0x40
    char *version;
    char *keypkgId;
    struct ami_name *owner;
    struct ami_pubkey_info *pubKeyInfo;
    struct ami_encr_privkey_info *encrPrivKeyInfo;
    struct ami_attr_list *keypkgAttrs;  /* optional */
    int usage;
    struct ami_tkey_list *tKeys;  /* optional */
} ami_keypkg_info;

typedef struct ami_keypkg {
    ami_keypkg_info info;
    struct ami_algid *algorithm;
    ami_bitstring   signature;
} ami_keypkg;

typedef struct ami_tkey_list {
    struct ami_tkey_list *next;
    struct ami_tkey *value;
} *ami_tkey_list;

typedef struct ami_tkey {
    unsigned char bit_mask;
#define	TrustedKey_extensions_present 0x80
    struct ami_name *owner;
    struct ami_pubkey_info *pubKeyInfo;
    struct ami_name *issuer;  /* NULL for not present */
    struct ami_validity *validity;  /* NULL for not present */
    struct ami_cert_serialnum *serial;  /* NULL for not present */
    struct ami_cert_extn_list *TrustedKey_extensions;  /* optional */
} ami_tkey;

typedef struct ami_serv_key_info {
    Any keyAlgId;
    int uid;
    int flags;
    Any privKey;
    char *keypkgId;
    char *hostIP;
    Any keypkg;
} ami_serv_key_info;

typedef struct _octet1 {
    unsigned int    length;
    unsigned char   *value;
} _octet1;

typedef struct ami_digest_info {
    struct ami_algid *digestAlgorithm;
    _octet1 digest;
} ami_digest_info;

typedef struct ami_crl_set {
    struct ami_crl_set *next;
    struct ami_crl  *value;
} *ami_crl_set;

typedef struct ami_crl_entry {
    int userCertificate;
    UTCTime *revocationDate;
} ami_crl_entry;

typedef struct ami_crl_info {
    unsigned char bit_mask;
#define	CertificateRevocationListInfo_revokedCertificates_present 0x80
    struct ami_algid *signature;
    struct ami_name *issuer;
    UTCTime *lastUpdate;
    UTCTime  *nextUpdate;
	struct _seqof2 {
		struct _seqof2 *next;
		ami_crl_entry value;
	} *CertificateRevocationListInfo_revokedCertificates;
} ami_crl_info;

typedef struct ami_crl {
    ami_crl_info info;
    struct ami_algid *algorithm;
    ami_bitstring signature;
} ami_crl;

typedef struct ami_pbe_param {
	struct {
		unsigned short  length;
		unsigned char   value[8];
	} salt;
    int iterationCount;
} ami_pbe_param;

typedef struct ami_extcert_info {
    int version;
    struct ami_cert *certificate;
    struct ami_attr_list *attributes;
} ami_extcert_info;

typedef struct ami_extcert {
    struct ami_extcert_info *extendedCertificateInfo;
    struct ami_algid *signatureAlgorithm;
    ami_bitstring signature;
} ami_extcert;

typedef struct ami_extcerts_and_certs {
    struct ami_extcerts_and_certs *next;
    struct ami_extcert_or_cert *value;
} *ami_extcerts_and_certs;

typedef struct ami_extcert_or_cert {
    unsigned short choice;
#define	cert_chosen 1
#define	 extendedCert_chosen 2
	union {
		struct ami_cert *cert;
		struct ami_extcert *extendedCert;
	} u;
} ami_extcert_or_cert;

typedef Any Content;

typedef struct ami_content_info {
    struct ami_oid *contentType;
    Content *content;  /* NULL for not present */
} ami_content_info;

typedef struct ami_content_info_fm {
    struct ami_oid *contentType;
    Content *content;  /* NULL for not present */
} ami_content_info_fm;

typedef struct ami_enveloped_data {
    int version;
    struct ami_rcpt_info_list *recipientInfos;
    struct ami_encr_content_info *encryptedContentInfo;
} ami_enveloped_data;

typedef struct ami_encr_data {
    int version;
    struct ami_encr_content_info *encryptedContentInfo;
} ami_encr_data;

typedef struct ami_signed_data {
    unsigned char bit_mask;
#define	SignedData_certs_present 0x80
#define	SignedData_crls_present 0x40
    int version;
    struct ami_digest_alg_list *digestAlgorithms;
    struct ami_content_info *contentInfo;
    struct ami_extcerts_and_certs *SignedData_certs;  /* optional */
    struct ami_crl_set *SignedData_crls;  /* optional */
    struct ami_signer_info_list *signerInfos;
} ami_signed_data;

typedef struct ami_signed_data_fm {
    unsigned char bit_mask;
#define	SignedDataFm_certs_present 0x80
#define	SignedDataFm_crls_present 0x40
    int version;
    struct ami_digest_alg_list *digestAlgorithms;
    struct ami_content_info_fm *contentInfo;
    struct ami_extcerts_and_certs *SignedDataFm_certs;  /* optional */
    struct ami_crl_set *SignedDataFm_crls;  /* optional */
    struct ami_signer_info_list *signerInfos;
} ami_signed_data_fm;

typedef struct ami_rcpt_info_list {
    struct ami_rcpt_info_list *next;
    struct ami_rcpt_info *value;
} *ami_rcpt_info_list;

typedef struct ami_encr_content_info {
    struct ami_oid *contentType;
    struct ami_algid *contentEncryptionAlgorithm;
    struct ami_encr_content *encryptedContent;  /* NULL for not present */
} ami_encr_content_info;

typedef struct ami_pkcs_data {
    unsigned int length;
    unsigned char *value;
} ami_pkcs_data;

typedef struct ami_pkcs_data_fm {
    unsigned int length;
    unsigned char *value;
} ami_pkcs_data_fm;

typedef struct ami_encr_content {
    unsigned int length;
    unsigned char *value;
} ami_encr_content;

typedef struct ami_rcpt_info {
    int version;
    struct ami_issuer_and_serialnum *issuerAndSerialNumber;
    struct ami_algid *keyEncryptionAlgorithm;
    _octet1 encryptedKey;
} ami_rcpt_info;

typedef struct ami_signer_info {
    unsigned char bit_mask;
#define	authenticatedAttributes_present 0x80
#define	unauthenticatedAttributes_present 0x40
    int version;
    struct ami_issuer_and_serialnum *issuerAndSerialNumber;
    struct ami_algid *digestAlgorithm;
    struct ami_attr_list *authenticatedAttributes;  /* optional */
    struct ami_algid *digestEncryptionAlgorithm;
    _octet1 encryptedDigest;
    struct ami_attr_list *unauthenticatedAttributes;  /* optional */
} ami_signer_info;

typedef struct ami_signer_info_list {
    struct ami_signer_info_list *next;
    struct ami_signer_info *value;
} *ami_signer_info_list;

typedef struct ami_issuer_and_serialnum {
    struct ami_name *issuer;
    ami_cert_serialnum serial;
} ami_issuer_and_serialnum;

typedef struct ami_digest_alg_list {
    struct ami_digest_alg_list *next;
    struct ami_algid *value;
} *ami_digest_alg_list;

typedef struct ami_privkey_info {
    unsigned char   bit_mask;
#define	attributes_present 0x80
    int version;
    struct ami_algid *privateKeyAlgorithm;
    _octet1 privateKey;
    struct ami_attr_list *attributes;  /* optional */
} ami_privkey_info;

typedef struct ami_encr_privkey_info {
    struct ami_algid *encryptionAlgorithm;
    ami_octetstring encryptedData;
} ami_encr_privkey_info;

typedef struct ami_certreq_info {
    int version;
    struct ami_name *subject;
    struct ami_pubkey_info *pubKeyInfo;
    struct ami_attr_list *attributes;
} ami_certreq_info;

typedef struct ami_certreq {
    ami_certreq_info info;
    struct ami_algid *algorithm;
    ami_bitstring   signature;
} ami_certreq;

typedef struct ami_challenge_pwd {
    unsigned short  choice;
#define	ChallengePassword_printableString_chosen 1
#define	ChallengePassword_t61String_chosen 2
	union {
		char *ChallengePassword_printableString;
		char *ChallengePassword_t61String;
	} u;
} ami_challenge_pwd;

typedef char *ami_email_addr;

typedef struct ami_pubkey_and_challenge {
	struct ami_pubkey_info *spki;
	char *challenge;
} ami_pubkey_and_challenge;

typedef struct ami_signed_pubkey_and_challenge {
    ami_pubkey_and_challenge pubKeyAndChallenge;
    struct ami_algid *sigAlg;
    ami_bitstring   signature;
} ami_signed_pubkey_and_challenge;

/* Algorithm types */
typedef enum {
	AMI_OTHER_ALG = -1,
	AMI_SYM_ENC_ALG,
	AMI_ASYM_ENC_ALG,
	AMI_HASH_ALG,
	AMI_SIG_ALG,
	AMI_KEYED_INTEGRITY_ALG
} ami_alg_type;

/* Parameter types */
typedef enum {
	AMI_PARM_OTHER = -1,
	AMI_PARM_ABSENT,
	AMI_PARM_INTEGER,
	AMI_PARM_OCTETSTRING,
	AMI_PARM_NULL,
	AMI_PARM_RC2_CBC,
	AMI_PARM_PBE
} ami_parm_type;

/* Algorithm table */
#define	AMI_NO_EXPORT_KEYSIZE_LIMIT	0
typedef struct ami_alg_list {
	ami_oid		*oid;
	char		*name;
	ami_alg_type	algType;
	ami_parm_type	parmType;
	size_t		keysize_limit;
} ami_alg_list;

typedef ami_name *ami_name_ptr;
typedef ami_algid *ami_algid_ptr;
typedef ami_pubkey_info *ami_pubkey_info_ptr;
typedef ami_validity *ami_validity_ptr;
typedef ami_cert *ami_cert_ptr;
typedef ami_cert_extn *ami_cert_extn_ptr;
typedef ami_cert_pair *ami_cert_pair_ptr;
typedef ami_attr *ami_attr_ptr;
typedef ami_keypkg *ami_keypkg_ptr;
typedef ami_tkey *ami_tkey_ptr;
typedef ami_digest_info *ami_digest_info_ptr;
typedef ami_certreq *ami_certreq_ptr;
typedef ami_enveloped_data *ami_enveloped_data_ptr;

/*
 * OIDs
 */
extern ami_oid *AMI_MD2_OID;
extern ami_oid *AMI_MD4_OID;
extern ami_oid *AMI_MD5_OID;
extern ami_oid *AMI_SHA_1_OID;
extern ami_oid *AMI_RSA_ENCR_OID;
extern ami_oid *AMI_MD2WithRSAEncryption_OID;
extern ami_oid *AMI_MD5WithRSAEncryption_OID;
extern ami_oid *AMI_SHA1WithRSAEncryption_OID;
extern ami_oid *AMI_DSA_OID;
extern ami_oid *AMI_SHA1WithDSASignature_OID;
extern ami_oid *AMI_DES_ECB_OID;
extern ami_oid *AMI_DES_CBC_OID;
extern ami_oid *AMI_DES3_CBC_OID;
extern ami_oid *AMI_DES_MAC_OID;
extern ami_oid *AMI_RC2_CBC_OID;
extern ami_oid *AMI_RC4_OID;
extern ami_oid *AMI_RSA_OID;
extern ami_oid *AMI_PbeWithMD5AndDES3_CBC_OID;
extern ami_oid *commonName_OID;
extern ami_oid *serial_OID;
extern ami_oid *countryName_OID;
extern ami_oid *locality_OID;
extern ami_oid *state_OID;
extern ami_oid *streetAddress_OID;
extern ami_oid *orgname_OID;
extern ami_oid *orgunit_OID;
extern ami_oid *description_OID;
extern ami_oid *emailAddress_OID;
extern ami_oid *unstructuredName_OID;
extern ami_oid *contentType_OID;
extern ami_oid *messageDigest_OID;
extern ami_oid *signingTime_OID;
extern ami_oid *countersignature_OID;
extern ami_oid *challengePassword_OID;
extern ami_oid *unstructuredAddress_OID;
extern ami_oid *extendedCertificateAttributes_OID;
extern ami_oid *data_OID;
extern ami_oid *signedData_OID;
extern ami_oid *envelopedData_OID;
extern ami_oid *signedAndEnvelopedData_OID;
extern ami_oid *digestedData_OID;
extern ami_oid *encryptedData_OID;

/*
 * Misc. AlgIDs
 */
extern struct ami_algid *AMI_RSA_ENCR_AID;
extern struct ami_algid *AMI_MD2WithRSAEncryption_AID;
extern struct ami_algid *AMI_MD5WithRSAEncryption_AID;
extern struct ami_algid *AMI_SHA1WithRSAEncryption_AID;
extern struct ami_algid *AMI_DSA_AID;
extern struct ami_algid *AMI_SHA1WithDSASignature_AID;
extern struct ami_algid *AMI_DH_AID;
extern struct ami_algid *AMI_MD2_AID;
extern struct ami_algid *AMI_MD4_AID;
extern struct ami_algid *AMI_MD5_AID;
extern struct ami_algid *AMI_SHA1_AID;
extern struct ami_algid *AMI_RC4_AID;

#define	AMI_DEFAULT_SIG_OID	AMI_MD5WithRSAEncryption_OID
#define	AMI_DEFAULT_SIG_AID	AMI_MD5WithRSAEncryption_AID

#define	AMI_DES_NAME		"des"
#define	AMI_DES_EXP_NAME	"des_exp"
#define	AMI_DES_EDE_NAME	"des3"
#define	AMI_RC2_NAME		"rc2"
#define	AMI_RC2_EXP_NAME	"rc2_exp"
#define	AMI_RC4_NAME		"rc4"
#define	AMI_RC4_EXP_NAME	"rc4_exp"
#define	AMI_RSA_NAME		"rsa"
#define	AMI_RSA_1024_NAME	"rsa (1024)"
#define	AMI_RSAENCRYPTION_NAME	"rsaEncryption"
#define	AMI_RSAENCRYPTION_1024_NAME	"rsaEncryption (1024)"
#define	AMI_MD2_WITH_RSA_NAME	"md2WithRSA"
#define	AMI_MD5_WITH_RSA_NAME	"md5WithRSA"
#define	AMI_SHA1_WITH_RSA_NAME	"sha1WithRSA"
#define	AMI_MD2_WITH_RSAENCRYPTION_NAME	"md2WithRSAEncryption"
#define	AMI_MD5_WITH_RSAENCRYPTION_NAME	"md5WithRSAEncryption"
#define	AMI_SHA1_WITH_RSAENCRYPTION_NAME "sha1WithRSAEncryption"
#define	AMI_SHA1_WITH_DSASIGNATURE_NAME	"sha1WithDSASignature"
#define	AMI_MD2_NAME		"md2"
#define	AMI_MD5_NAME		"md5"
#define	AMI_SHA1_NAME		"sha1"

extern struct ami_alg_list amiAlgList[];
extern struct AttrList attrList[];

/*
 * encoded keypkg data
 */
typedef struct ami_data_set_t {
	uchar_t * data;		/* encoded keypkg data */
	size_t data_len;	/* encoded keypkg data length */
} ami_data_set;

/* a linked list of keypkgs or certs */
typedef struct _set_item {
	void *data;
	size_t data_len;
	struct _set_item *next;
} set_item;

typedef struct _item_list {
	void *data;
	struct _item_list *next;
} item_list;

typedef struct {
    long           length;
    unsigned char *value;
} ami_buf;

/*
 * AMI handle
 */
typedef struct ami_handle {
	char *backend;			/* Backend nameservice for keystore */
	char *keypkg_id;		/* keystore alias for amiserv */
	char *host_ip;			/* host IP for amiserv */
	unsigned long hCkSession;	/* Cryptoki session handle */
	int smart_card_params;		/* smartcard params */
	struct ami_algid *encr_alg;	/* Encryption algorithm */
	struct ami_algid *dig_alg;	/* Digest algorithm */
	int dig_mech;			/* Digest mechanism */
	void *dhKeyAgreeAlg;		/* DH key agreement algrithm */
	nl_catd fd;			/* i18n */
	int add_data_flag;		/* For AMI_ADD_DATA flag */
	void *clnt;			/*
					 * RPC client handle
					 * can not be defined as a
					 * CLIENT * because then we
					 * have to #include rpc.h
					 * which causes a multiply
					 * defined symbol in BSAFE
					 * (T_CALL)
					 */
	void *hCkContext;		/* Cryptoki session context */
} ami_handle;

/*
 * Parameters for RSA key generation
 */
typedef struct ami_rsa_keygen_param_t {
	uint_t modulusBits;
	uchar_t *publicExponent; /* const */
	size_t publicExponentLen;
} ami_rsa_keygen_param;

typedef struct ami_des_keygen_param_t {
	uchar_t *saltVal; /* const */
	size_t saltLen;
	char *passwd; /* const */
	int iterationCount;
} ami_des_keygen_param;

/* Structure to hold symmetric keys */
typedef struct ami_session_key {
	uint_t mechanism;
	uchar_t *key;
	uint_t length;
} ami_session_key;

typedef struct ami_private_key {
	char *alias;
	int mech;
	uchar_t *key;
	size_t length;
} ami_private_key;

typedef struct ami_public_key {
	int mech;
	uchar_t *key;
	size_t length;
} ami_public_key;

typedef ami_alg_params ami_session_key_params;

typedef struct ami_nss_cert_list {
	ami_cert *certs;
	int count;
	int current;
} ami_nss_cert_list;

#ifdef	__cplusplus
}
#endif

#endif	/* _AMI_LOCAL_H */
