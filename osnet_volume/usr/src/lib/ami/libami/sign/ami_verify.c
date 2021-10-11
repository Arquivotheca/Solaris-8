/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)ami_verify.c	1.3 99/07/16 SMI"

#include <stdlib.h>
#include "ami_local.h"
#include "ami.h"
#include "ami_proto.h"
#include "cryptoki.h"
#include "cryptoki_util_proto.h"

extern B_ALGORITHM_METHOD *chooser[];

static AMI_STATUS
dsa_verify(ami_handle_t, const uchar_t *, const size_t, const int,
    const ami_algid *, const uchar_t *, const size_t,
    const ami_algid *, const uchar_t *, const size_t);


AMI_STATUS
ami_verify(
    ami_handle_t amih,	/* IN: AMI handle */
    const uchar_t * pData,	/* IN: data to be verified */
    size_t dataLen,		/* IN: data length */
    int flag,			/* IN: more input data flag */
    ami_mechanism keyMech,	/* IN: verification key algorithm */
    const ami_public_key_t key,	/* IN: public key */
    ami_mechanism sigMech,	/* IN: signature algorithm */
    const uchar_t *pSigValue,	/* IN: signature */
    size_t sigLen		/* IN: signature length */
)
{
	CK_MECHANISM mechanism = {0};
	CK_SESSION_HANDLE hSession = 0;
	CK_ATTRIBUTE_PTR pPublicKeyTemplate = NULL;
	CK_USHORT usPublicKeyAttributeCount = 0;
	CK_OBJECT_HANDLE hPublicKey = 0;
	ami_alg_type integAlgType;
	uchar_t *encodedData = NULL, *pDigest = NULL;
	size_t encodedLen = 0, digestLen = 0;
	AMI_STATUS ami_status = AMI_EPARM;
	int equal;
	char digestAlgo[5];
	ami_algid *pSigAlg, *pKeyAlg;
	uchar_t *pKey = 0;	/* IN: verification key */
	size_t keyLen = 0;	/* IN: verification key length */

	if (key) {
		pKey = key->key;
		keyLen = key->length;
	}

	if (!amih || (!sigMech && (amih->encr_alg == NULL ||
	    amih->dig_alg == NULL)) ||
	    !keyMech || !pKey || !keyLen || !pSigValue || !sigLen)
		return (AMI_EPARM);

	/*
	 * If input data is provided, it must have a length. Otherwise, its
	 * length must be zero.
	 */
	if ((!pData && dataLen != 0) ||
	    (pData != 0 && dataLen == 0))
		return (AMI_EPARM);

	/* If key is present, get the algid */
	if (pKey) {
		switch (keyMech) {
		case AMI_RSA:
			pKeyAlg = AMI_RSA_ENCR_AID;
			if (sigMech == AMI_SHA1WithDSASignature)
				return (AMI_VERIFY_ERR);
			break;
		case AMI_DSA:
			pKeyAlg = AMI_SHA1WithDSASignature_AID;
			if (sigMech != AMI_SHA1WithDSASignature)
				return (AMI_VERIFY_ERR);
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
	 * Check if supplied integrity algorithm is indeed an integrity
	 * algorithm
	 */
	if ((ami_status = __ami_alg2AlgType(pSigAlg->algorithm,
	    &integAlgType)) != AMI_OK)
		goto out;
	if (integAlgType != AMI_SIG_ALG &&
	    integAlgType != AMI_KEYED_INTEGRITY_ALG) {
		/* Wrong algorithm type */
		ami_status = AMI_EPARM;
		goto out;
	}

	/*
	 * Store the verification algorithm in the AMI handle, so that changing
	 * the verification algorithm does not have any impact on the
	 * verification operation
	 */
	if (amih->encr_alg == NULL) {
		if ((ami_status = __ami_cmp_oid(pSigAlg->algorithm,
		    AMI_MD2WithRSAEncryption_OID,
		    &equal)) == AMI_OK && equal == 0) {
			strcpy(digestAlgo, "MD2");
			amih->dig_alg = AMI_MD2_AID;
			amih->dig_mech = AMI_MD2;
			amih->encr_alg = AMI_RSA_ENCR_AID;
		} else if ((ami_status = __ami_cmp_oid(pSigAlg->algorithm,
		    AMI_MD5WithRSAEncryption_OID,
		    &equal)) == AMI_OK && equal == 0) {
			strcpy(digestAlgo, "MD5");
			amih->dig_alg = AMI_MD5_AID;
			amih->dig_mech = AMI_MD5;
			amih->encr_alg = AMI_RSA_ENCR_AID;
		} else if ((ami_status = __ami_cmp_oid(pSigAlg->algorithm,
		    AMI_SHA1WithRSAEncryption_OID,
		    &equal)) == AMI_OK && equal == 0) {
			strcpy(digestAlgo, "SHA1");
			amih->dig_alg = AMI_SHA1_AID;
			amih->dig_mech = AMI_SHA1;
			amih->encr_alg = AMI_RSA_ENCR_AID;
		} else if ((ami_status = __ami_cmp_oid(pSigAlg->algorithm,
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

	/* Branch here to a DSA-specific verify function */
	if (amih->encr_alg == AMI_SHA1WithDSASignature_AID) {
		ami_status = dsa_verify(amih, pData, dataLen, flag, pKeyAlg,
		    pKey, keyLen, pSigAlg, pSigValue,
		    sigLen);
		goto out;
	}

	/*
	 * If data sent is not digested, digest the data
	 */
	if ((flag != AMI_DIGESTED_DATA) &&
	    (flag != AMI_DIGESTED_AND_ENCODED_DATA)) {
		/* Digest the data */
		if ((ami_status = ami_digest(amih, pData, dataLen,
		    flag, amih->dig_mech, &pDigest, &digestLen)) != AMI_OK)
			goto out;
		/* No need to encode the digested data */
	} else {
		pDigest = (uchar_t *) malloc(dataLen * sizeof (uchar_t));
		memcpy(pDigest, pData, dataLen);
		digestLen = dataLen;
	}

	/* Encode the data */
	if (flag != AMI_DIGESTED_AND_ENCODED_DATA) {
		if ((ami_status = __ami_rpc_encode_digested_data(
		    pDigest, digestLen, (uchar_t *)
		    digestAlgo, &encodedData, &encodedLen))
		    != AMI_OK)
			goto out;
		free(pDigest);
	} else {
		encodedData = pDigest;
		encodedLen = dataLen;
	}

	if (flag == AMI_ADD_DATA) {
		ami_status = AMI_OK;
		goto out;
	}

	/*
	 * Make sure the algorithm associated with the verification key is
	 * compatible with the requested verification algorithm
	 */

	/* Create template for verification key */
	if ((ami_status = __ami_cmp_oid(pKeyAlg->algorithm,
	    AMI_RSA_ENCR_OID, &equal)) == AMI_OK && equal == 0) {
		if (subjectPublicKeyInfoBer_2_template((uchar_t *) pKey,
		    keyLen, &pPublicKeyTemplate, &usPublicKeyAttributeCount)
		    != CKR_OK) {
			ami_status = AMI_BAD_KEY;
			goto out;
		}
	} else {
		if (equal != 0)
			/* Unsupported verification key algorithm */
			ami_status = AMI_UNSUPPORTED_KEY_TYPE;
		goto out;
	}

	/*
	 * Set the Cryptoki mechanism
	 */
	if ((ami_status = __ami_cmp_oid(amih->encr_alg->algorithm,
	    AMI_RSA_ENCR_OID, &equal)) == AMI_OK &&
	    equal == 0) {
		mechanism.mechanism = CKM_RSA_PKCS;
		/* Does not have a parameter (see PKCS#11) */
		mechanism.pParameter = NULL;
		mechanism.usParameterLen = 0;
	} else {
		if (equal != 0)
			/* Unsupported verification algorithm */
			ami_status = AMI_ALGORITHM_UNKNOWN;
		goto out;
	}

	/* Create Cryptoki session */
	if (C_OpenSession((CK_SLOT_ID) 0, (CK_FLAGS) 0,
		(CK_VOID_PTR) 0, nullNotify,
		&hSession) != CKR_OK) {
		ami_status = AMI_SYSTEM_ERR;
		goto out;
	}

	/* Create key object for verification key */
	if (C_CreateObject(hSession, pPublicKeyTemplate,
		usPublicKeyAttributeCount,
		&hPublicKey) != CKR_OK) {
		ami_status = AMI_SYSTEM_ERR;
		goto out;
	}

	if (C_VerifyInit(hSession, &mechanism, hPublicKey) != CKR_OK) {
		ami_status = AMI_VERIFY_ERR;
		goto out;
	}

	/* Verify */
	if (C_Verify(hSession, encodedData, (CK_USHORT) encodedLen,
		(uchar_t *) pSigValue, sigLen) != CKR_OK) {
		ami_status = AMI_VERIFY_ERR;
		goto out;
	}
	/* success */
	ami_status = AMI_OK;

out:
	if (pDigest)
		free(pDigest);

	if ((flag == AMI_END_DATA) ||
	    (flag == AMI_DIGESTED_DATA) ||
	    (flag == AMI_DIGESTED_AND_ENCODED_DATA)) {
		amih->dig_alg = 0;
		amih->encr_alg = 0;
		amih->dig_mech = 0;
	}
	/* Clear the session and its key object */
	if (hPublicKey)
		C_DestroyObject(hSession, hPublicKey);
	if (hSession)
		C_CloseSession(hSession);

	return (ami_status);
}

static AMI_STATUS
dsa_verify(
    ami_handle_t amih,		/* IN: AMI handle */
    const uchar_t *pData,	/* IN: data to be verified */
    const size_t dataLen,	/* IN: data length */
    const int flag,		/* IN: more input data flag */
    const ami_algid *pKeyAlg,	/* IN: verification key algorithm */
    const uchar_t *pKey,	/* IN: verification key */
    const size_t keyLen,	/* IN: verification key length */
    const ami_algid *pSigAlg,	/* IN: signature algorithm */
    const uchar_t *pSigValue,	/* IN: signature */
    const size_t sigLen		/* IN: signature length */
)
{
	AMI_STATUS ami_status;
	ITEM dsa_publicKeyItem;
	B_KEY_OBJ dsa_publicKeyObj = NULL_PTR;
	int b_status;
	B_ALGORITHM_OBJ algorithmObject = NULL_PTR;
	int equal;

	ami_status = AMI_BAD_KEY;

	if ((ami_status = __ami_cmp_oid(pKeyAlg->algorithm,
	    AMI_DSA_OID, &equal)) == AMI_OK &&
	    equal == 0) {
		ami_status = AMI_BAD_KEY;
		b_status = B_CreateKeyObject(&dsa_publicKeyObj);
		if (b_status != 0) {
			goto out;
		}
		dsa_publicKeyItem.data = (uchar_t *)pKey;
		dsa_publicKeyItem.len = keyLen;
		b_status = B_SetKeyInfo(dsa_publicKeyObj,
		    KI_DSAPublicBER,
		    (POINTER) & dsa_publicKeyItem);
		if (b_status != 0) {
			goto out;
		}
	} else {
		goto out;
	}

	ami_status = AMI_VERIFY_ERR;

	b_status = B_CreateAlgorithmObject(&algorithmObject);
	if (b_status != 0) {
		goto out;
	}

	/* If the data has already been digested, just use AI_DSA */
	if (flag == AMI_DIGESTED_DATA) {
	    b_status = B_SetAlgorithmInfo(
		algorithmObject, AI_DSA, NULL_PTR);
	} else {
	    /* set algorithm object to SHA with DSA signature */
	    b_status = B_SetAlgorithmInfo(
		algorithmObject, AI_DSAWithSHA1, NULL_PTR);
	}
	if (b_status != 0) {
		goto out;
	}
	b_status = B_VerifyInit(
	    algorithmObject, dsa_publicKeyObj,
	    chooser, (A_SURRENDER_CTX *) NULL);
	if (b_status != 0) {
		goto out;
	}
	b_status = B_VerifyUpdate(
	    algorithmObject, (uchar_t *)pData, dataLen,
	    (A_SURRENDER_CTX *) NULL);
	if (b_status != 0) {
		goto out;
	}
	b_status = B_VerifyFinal(
	    algorithmObject, (uchar_t *)pSigValue, sigLen,
	    (B_ALGORITHM_OBJ) NULL, (A_SURRENDER_CTX *) NULL);
	if (b_status != 0) {
		goto out;
	}
	ami_status = AMI_OK;

out:
	B_DestroyKeyObject(&dsa_publicKeyObj);
	B_DestroyAlgorithmObject(&algorithmObject);

	return (ami_status);
}
