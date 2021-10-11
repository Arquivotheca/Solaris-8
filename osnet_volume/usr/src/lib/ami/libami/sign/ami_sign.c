/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)ami_sign.c	1.3 99/08/04 SMI"

#include <stdlib.h>
#include <unistd.h>
#include "cryptoki.h"
#include "cryptoki_util_proto.h"
#include "ami_local.h"
#include "ami.h"
#include "ami_proto.h"

extern B_ALGORITHM_METHOD *chooser[];

AMI_STATUS
ami_sign(ami_handle_t amih,	/* IN:  ami handle */
    const uchar_t * pData,	/* IN:  data to be signed */
    size_t dataLen,		/* IN:  data length */
    int flag,			/* IN:  more input data flag */
    ami_mechanism keyMech,	/* IN:  signature key algorithm */
    ami_private_key_t key,	/* IN:  signature key object */
    ami_mechanism sigMech,	/* IN:  signature algorithm */
    uchar_t ** ppSigValue,	/* OUT: signature */
    size_t * pSigLen		/* OUT: signature length */
)
{
	AMI_STATUS ami_status = AMI_EPARM;
	uchar_t *pSigValue = NULL;
	uint_t sigLen = 0;
	uchar_t *encodedData = NULL, *pDigest = NULL;
	size_t encodedLen = 0, digestLen = 0;
	char digestAlgo[5];
	int equal;
	char signAlgo[20];
	ami_algid *pSigAlg, *pKeyAlg;
	uchar_t * pKey = 0;	/* IN:  signature key */
	size_t keyLen = 0;	/* IN:  signature key length */

	if (!amih || (!sigMech && (amih->encr_alg == NULL ||
	    amih->dig_alg == NULL)) ||
	    flag != AMI_END_DATA ||
	    !ppSigValue || !pSigLen)
		return (AMI_EPARM);

	/*
	 * If input data is provided, it must have a length. Otherwise, its
	 * length must be zero.
	 */
	if ((!pData && dataLen != 0) || (pData != 0 && dataLen == 0))
		return (AMI_EPARM);

	/* If an output buffer is provided, make sure there's a length */
	if (flag == AMI_END_DATA && ((*ppSigValue && *pSigLen == 0) ||
	    (*ppSigValue == 0 && *pSigLen)))
		return (AMI_EPARM);

	/* Check if private key is provided */
	if (key) {
		pKey = key->key;
		keyLen = key->length;
	}

	/* If a signature key is provided, it must have a length */
	if ((pKey && keyLen == 0) ||(!pKey && keyLen != 0))
		return (AMI_EPARM);

	/*
	 * Make sure that: If a signature key algorithm is present, a signature
	 * key must be present, too. If a signature key is present, a signature
	 * key algorithm must be present, too.
	 */
	if ((keyMech && !pKey) ||(pKey && !keyMech))
		return (AMI_EPARM);

	/* If key is present, get the algid */
	if (pKey) {
		switch (keyMech) {
		case AMI_RSA:
			pKeyAlg = AMI_RSA_ENCR_AID;
			break;
		case AMI_DSA:
			pKeyAlg = AMI_DSA_AID;
			break;
		default:
			return (AMI_ALGORITHM_UNKNOWN);
		}
	}

	/* Get the signature algorithm */
	switch (sigMech) {
	case AMI_MD2WithRSAEncryption:
		pSigAlg = AMI_MD2WithRSAEncryption_AID;
		break;
	case AMI_MD5WithRSAEncryption:
		pSigAlg = AMI_MD5WithRSAEncryption_AID;
		break;
	case AMI_SHA1WithRSAEncryption:
		pSigAlg = AMI_SHA1WithRSAEncryption_AID;
		break;
	case AMI_SHA1WithDSASignature:
		pSigAlg = AMI_SHA1WithDSASignature_AID;
		break;
	default:
		return (AMI_ALGORITHM_UNKNOWN);
	}


	/*
	 * Store the signature algorithm in the AMI handle, so that changing
	 * the signature algorithm has no impact on the signature operation.
	 */
	if (amih->encr_alg == NULL) {
		if ((ami_status = __ami_cmp_oid(
		    pSigAlg->algorithm,
		    AMI_MD2WithRSAEncryption_OID,
		    &equal)) == AMI_OK && equal == 0) {
			strcpy(digestAlgo, "MD2");
			amih->dig_alg = AMI_MD2_AID;
			amih->dig_mech = AMI_MD2;
			amih->encr_alg = AMI_RSA_ENCR_AID;
		} else if ((ami_status = __ami_cmp_oid(
		    pSigAlg->algorithm,
		    AMI_MD5WithRSAEncryption_OID,
		    &equal)) == AMI_OK && equal == 0) {
			strcpy(digestAlgo, "MD5");
			amih->dig_alg = AMI_MD5_AID;
			amih->dig_mech = AMI_MD5;
			amih->encr_alg = AMI_RSA_ENCR_AID;
		} else if ((ami_status = __ami_cmp_oid(
		    pSigAlg->algorithm,
		    AMI_SHA1WithRSAEncryption_OID,
		    &equal)) == AMI_OK && equal == 0) {
			strcpy(digestAlgo, "SHA1");
			amih->dig_alg = AMI_SHA1_AID;
			amih->dig_mech = AMI_SHA1;
			amih->encr_alg = AMI_RSA_ENCR_AID;
		} else if ((ami_status = __ami_cmp_oid(
		    pSigAlg->algorithm,
		    AMI_SHA1WithDSASignature_OID,
		    &equal)) == AMI_OK && equal == 0) {
			amih->dig_alg = AMI_SHA1_AID;
			amih->dig_mech = AMI_SHA1;
			amih->encr_alg = AMI_SHA1WithDSASignature_AID;
		} else {
			if (ami_status == AMI_OK)
				/* Unsupported signature algorithm */
				ami_status = AMI_ALGORITHM_UNKNOWN;
			goto out;
		}
	}

	/*
	 * If caller provided memory for signature, we pass it to the lower
	 * level routines rpc_sign()/__ami_sign1().
	 */
	if (*ppSigValue != 0) {
		pSigValue = *ppSigValue;
		sigLen = *pSigLen;
	}

	/*
	 * NULL-KEY INTERCEPT STRATEGY:
	 *
	 * If no key is provided, we must reroute this request. This is rather
	 * convoluted, because we cannot merely ask AMI to give us the key so
	 * processing can continue here.
	 *
	 * This is an RPC call to the ami keyserver.
	 *
	 * Thus called with all the prerequisite data, ami_sign.c can now sign
	 * the data, which it does, and returns.  ami_sign.c will then again be
	 * re-entered as part of the user-side return sequence, just after the
	 * original point the null-key was intercepted, with a buffer filled
	 * with the signed data.  From there, it merely returns the signed data
	 * to the original caller of the API.
	 */
	if (!pKey) {
		char *alias = 0;
		if (pSigAlg == AMI_MD5WithRSAEncryption_AID)
			strcpy(signAlgo, "MD5/RSA");
		else if (pSigAlg == AMI_MD2WithRSAEncryption_AID)
			strcpy(signAlgo, "MD2/RSA");
		else if (pSigAlg == AMI_SHA1WithRSAEncryption_AID)
			strcpy(signAlgo, "SHA1/RSA");
		else if (pSigAlg == AMI_SHA1WithDSASignature_AID)
			strcpy(signAlgo, "SHA/DSA");
		else
			return (AMI_EPARM);

		if (amih->backend && amih->keypkg_id) {
			alias = (char *) calloc(strlen(amih->backend) +
			    strlen(amih->keypkg_id) + 1, sizeof (char));
			strcpy(alias, amih->backend);
			strcat(alias, ":");
			strcat(alias, amih->keypkg_id);
		}

		/* RPC call always performs a digest on the data */
		if ((ami_status = __ami_rpc_sign_data(amih->clnt,
		    NULL, NULL,	amih->host_ip, getuid(), alias,
		    signAlgo, pData, dataLen,
		    (void **) &pSigValue, (int *) &sigLen)) != AMI_OK) {
			goto out;
		}
		if (alias)
			free(alias);
	} else {
		/* Check if the data is digested */
		if (flag != AMI_DIGESTED_DATA) {
			if ((ami_status = ami_digest(amih, pData, dataLen,
			    flag, amih->dig_mech, &pDigest, &digestLen))
			    != AMI_OK)
				goto out;
		} else {
			pDigest = (uchar_t *) malloc(dataLen);
			memcpy(pDigest, pData, dataLen);
		}

		/* Encode the digested data */
		if ((ami_status = __ami_rpc_encode_digested_data(
		    pDigest, digestLen, (uchar_t *)
		    digestAlgo, &encodedData, &encodedLen))
		    != AMI_OK)
			goto out;
		free(pDigest);

		if ((ami_status = __ami_sign1(
		    amih, encodedData, encodedLen,
		    (ami_algid *) pKeyAlg, (uchar_t *) pKey,
		    (uint_t) keyLen, amih->encr_alg,
		    &pSigValue,	&sigLen)) != AMI_OK)
			goto out;
		free(encodedData);
	}

	/* success */
	*pSigLen = (size_t) (sigLen);
	if (*ppSigValue == 0)
		*ppSigValue = pSigValue;
	ami_status = AMI_OK;

out:
	if (flag == AMI_END_DATA) {
		amih->dig_alg = 0;
		amih->encr_alg = 0;
	}
	if (ami_status != AMI_OK) {
		if (pSigValue && pSigValue != *ppSigValue)
			/* We allocated memory for the output buffer */
			free(pSigValue);
	}
	return (ami_status);
}

AMI_STATUS
__ami_sign1(
    ami_handle_t amih,	/* IN:  AMI handle */
    uchar_t * pData,		/* IN:  data to be signed */
    uint_t dataLen,		/* IN:  data length */
    const ami_algid *pKeyAlg,	/* IN:  signature key algorithm */
    uchar_t * pKey,		/* IN:  signature key */
    uint_t keyLen,		/* IN:  signature key length */
    const ami_algid *pEncrAlg,	/* IN:  signature(encryption) algorithm */
    uchar_t ** ppSigValue,	/* OUT: signature */
    uint_t * pSigLen		/* OUT: signature length */
)
{
	CK_MECHANISM mechanism = {0};
	CK_ATTRIBUTE attr;
	CK_SESSION_HANDLE hSession = 0;
	CK_OBJECT_HANDLE hKey = 0;
	CK_ATTRIBUTE_PTR pKeyTemplate = NULL;
	CK_USHORT usKeyAttributeCount;
	uint_t sigLen = 0;
	CK_BYTE_PTR pSigValue = NULL;
	CK_USHORT usSigLen = 0;
	int equal;
	AMI_STATUS ami_status = AMI_EPARM;

	if (!amih || !pData || dataLen == 0 || !pKeyAlg || !pKey ||
	    keyLen == 0 || !pEncrAlg || !ppSigValue || !pSigLen)
		return (AMI_EPARM);

	/*
	 * XXX: Make sure the algorithm associated with the signature key is
	 * compatible with the requested signature algorithm(e.g., you cannot
	 * generate a DSS signature with an RSA key).
	 */
	/*
	 * XXX: Once we support(keyed) symmetric integrity algorithms, we have
	 * to check for the key type first, and then for the PubKeyType
	 * (if the key type is AMI_ASYM_ENC_ALG).
	 */

	/*
	 * Generate template for signature key
	 */
	if ((ami_status = __ami_cmp_oid(pKeyAlg->algorithm,
	    AMI_RSA_ENCR_OID, &equal)) == AMI_OK && equal == 0) {
		/*
		 * Convert ASN.1 encoded PKCS #8 PrivateKeyInfo to
		 * Cryptoki key template
		 */
		if (privateKeyInfoBer_2_template(pKey, keyLen,
		    &pKeyTemplate, &usKeyAttributeCount)
		    != CKR_OK) {
			ami_status = AMI_BAD_KEY;
			goto out;
		}

		/*
		 * Determine the length in bytes of the RSA modulus. This tells
		 * you how many bytes need to be allocated for the signature.
		 */
		attr.type = CKA_MODULUS;
		if (get_attr_from_template(pKeyTemplate,
		    usKeyAttributeCount, &attr) != CKR_OK) {
			ami_status = AMI_BAD_KEY;
			goto out;
		}
		sigLen = attr.usValueLen;
	} else {
		if (equal != 0)
			/* Unsupported signature key algorithm */
			ami_status = AMI_UNSUPPORTED_KEY_TYPE;
		goto out;
	}

	/*
	 * Set the Cryptoki mechanism
	 */
	if ((ami_status = __ami_cmp_oid(pEncrAlg->algorithm,
	    AMI_RSA_ENCR_OID, &equal)) == AMI_OK &&
	    equal == 0) {
		mechanism.mechanism = CKM_RSA_PKCS;
		/* Does not have a parameter(see PKCS#11) */
		mechanism.pParameter = NULL;
		mechanism.usParameterLen = 0;
	} else {
		if (equal != 0)
			/* Unsupported signature algorithm */
			ami_status = AMI_ALGORITHM_UNKNOWN;
		goto out;
	}

	/*
	 * Check if caller provided memory for signature. If no memory was
	 * provided, we have to allocate it.
	 */
	if (*ppSigValue != 0) {
		/*
		 * Does the memory provided by the caller(if any) have the
		 * right size?
		 */
		if (*pSigLen != 0 && *pSigLen < sigLen)
			/* Provided memory for output buffer too small */
			return (AMI_EBUFSIZE);

		pSigValue = *ppSigValue;
	} else {
		pSigValue = (CK_BYTE_PTR) calloc(sigLen, sizeof (char));
		if (!pSigValue) {
			ami_status = AMI_ENOMEM;
			goto out;
		}
		*pSigLen = 0;
	}

	/* Create Cryptoki session */
	if (C_OpenSession((CK_SLOT_ID) 0, (CK_FLAGS) 0,
	    (CK_VOID_PTR) 0, nullNotify, &hSession) != CKR_OK) {
		ami_status = AMI_SYSTEM_ERR;
		goto out;
	}

	/* Create key object for signature key */
	if (C_CreateObject(hSession, pKeyTemplate,
	    usKeyAttributeCount, &hKey) != CKR_OK) {
		ami_status = AMI_SYSTEM_ERR;
		goto out;
	}
	/* Initialize signature operation */
	if (C_SignInit(hSession, &mechanism, hKey) != CKR_OK) {
		ami_status = AMI_SIGN_ERR;
		goto out;
	}
	if (C_Sign(hSession, pData, (CK_USHORT) dataLen, pSigValue,
	    &usSigLen) != CKR_OK) {
		ami_status = AMI_SIGN_ERR;
		goto out;
	}
	/* success */
	*pSigLen = usSigLen;
	if (*ppSigValue == 0)
		*ppSigValue = (uchar_t *) pSigValue;

	ami_status = AMI_OK;

out:
	/* Clear the session and its private key object */
	if (hKey)
		C_DestroyObject(hSession, hKey);
	if (hSession)
		C_CloseSession(hSession);

	if (ami_status != AMI_OK) {
		if (pSigValue && pSigValue != *ppSigValue)
			/* We allocated memory for the output buffer */
			free(pSigValue);
	}
	return (ami_status);
}
