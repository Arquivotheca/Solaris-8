/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)ami_digest.c	1.4 99/07/16 SMI"

#include <stdlib.h>
#include <sys/md5.h>
#include "ami_local.h"
#include "ami.h"
#include "ami_proto.h"
#include "cryptoki.h"

AMI_STATUS
ami_digest(
    ami_handle_t amih,		/* IN:  ami handle */
    const uchar_t * pData,	/* IN:  input data  */
    size_t dataLen,		/* IN:  length of data in bytes */
    int flag,			/* IN:	more input data flag */
    ami_mechanism mech,	/* IN:  digest algorithm */
    uchar_t ** ppDigest,	/* OUT: digest */
    size_t * pDigestLen		/* OUT: length of digest */
)
{
	MD5_CTX *hContext = NULL;
	CK_MECHANISM mechanism = {0};
	CK_SESSION_HANDLE hSession = 0;
	CK_BYTE_PTR pDigest = (CK_BYTE_PTR) 0;
	CK_USHORT usDigestlen = 0;
	size_t digestLen = 0;
	int equal, equal_md2 = 1, equal_md5 = 1, equal_sha1 = 1;
	ami_algid *pDigAlg;
	AMI_STATUS ami_status = AMI_EPARM;

	/* Set the digest algorithm */
	switch (mech) {
	case AMI_MD2:
		pDigAlg = AMI_MD2_AID;
		break;
	case AMI_MD5:
		pDigAlg = AMI_MD5_AID;
		break;
	case AMI_SHA1:
		pDigAlg = AMI_SHA1_AID;
		break;
	default:
		return (AMI_ALGORITHM_UNKNOWN);
	}

	if (!amih || !pDigAlg ||
	    (flag != AMI_ADD_DATA && flag != AMI_END_DATA) ||
	    (flag == AMI_END_DATA &&(!ppDigest || !pDigestLen)))
		return (AMI_EPARM);

	/*
	 * If input data is provided, it must have a length.
	 * Otherwise, its
	 * length must be zero.
	 */
	if ((pData == 0 && dataLen != 0) ||
	    (pData != 0 && dataLen == 0))
		return (AMI_EPARM);

	/*
	 * No input data only allowed when we finish
	 * a multiple-part digest
	 * operation COMMENTED OUT : NOT SURE WHAT IT IS DOING !
	 * CAUSES A
	 * PROBLEM FOR DIGESTING 0 LENGHT DATA
	 * if (!pData &&(flag == AMI_ADD_DATA ||
	 * (flag == AMI_END_DATA &&
	 * amih->add_data_flag == 0))) return (AMI_EPARM);
	 */

	if ((__ami_cmp_oid(pDigAlg->algorithm, AMI_MD2_OID,
	    &equal_md2) != AMI_OK || equal_md2 != 0) &&
	    (__ami_cmp_oid(pDigAlg->algorithm,
	    AMI_MD5_OID, &equal_md5) != AMI_OK || equal_md5 != 0) &&
	    (__ami_cmp_oid(pDigAlg->algorithm,
	    AMI_SHA_1_OID, &equal_sha1)	!= AMI_OK || equal_sha1 != 0)) {
		/* Unsupported digest algorithm */
		ami_status = AMI_ALGORITHM_UNKNOWN;
		goto out;
	}

	/*
	 * On first call to ami_digest(), set the digest
	 * mechanism, create a
	 * Cryptoki session handle, and store it in AMI handle.
	 */
	if ((amih->hCkSession == 0) &&
	    (amih->hCkContext == NULL)) {
		/*
		 * First call to ami_digest() in this session
		 */

		if (flag == AMI_ADD_DATA)
			amih->add_data_flag = 1;

		/* Set the digest mechanism */
		if (equal_md2 == 0) {
			mechanism.mechanism = CKM_MD2;
			amih->dig_alg = AMI_MD2_AID;
		} else if (equal_md5 == 0) {
			mechanism.mechanism = CKM_MD5;
			amih->dig_alg = AMI_MD5_AID;
			amih->hCkContext = (MD5_CTX *)
			    calloc(1, sizeof (MD5_CTX));
			hContext = amih->hCkContext;
			MD5Init(hContext);
		} else if (equal_sha1 == 0) {
			mechanism.mechanism = CKM_SHA_1;
			amih->dig_alg = AMI_SHA1_AID;
		} else {
			ami_status = AMI_ALGORITHM_UNKNOWN;
			goto out;
		}

		if (equal_md5 != 0) {
			if (pDigAlg->parameters) {	/* XXX */
				/*
				 * Before we assign the parameter
				 * associated with the
				 * digest algorithm to the Cryptoki
				 * mechanism, we would
				 * have to ASN.1 decode it(same as
				 * in __ami_encrypt()
				 * and __ami_decrypt()). Don't know
				 * of any digest
				 * algorithms that have a parameter
				 * associated, though.
				 */
				mechanism.pParameter =
				    pDigAlg->parameters->value;
				mechanism.usParameterLen =
				    pDigAlg->parameters->length;
			}
			/* Create Cryptoki session handle */
			if (C_OpenSession((CK_SLOT_ID) 0, (CK_FLAGS) 0,
			    (CK_VOID_PTR) 0, nullNotify,
			    &(amih->hCkSession)) != CKR_OK) {
				ami_status = AMI_SYSTEM_ERR;
				goto out;
			}
			hSession = amih->hCkSession;

			/* Initialize the digest operation */
			if (C_DigestInit(hSession, &mechanism) != CKR_OK) {
				ami_status = AMI_DIGEST_ERR;
				goto out;
			}
		}
	} else {
		if (equal_md5 != 0) {
			hSession = amih->hCkSession;
		} else {
			hContext = amih->hCkContext;
		}
	}

	/*
	 * Digest the data
	 */
	if (flag == AMI_ADD_DATA) {
		if (equal_md5 != 0) {
			if (C_DigestUpdate(hSession, (uchar_t *) pData,
			    (CK_USHORT) dataLen) != CKR_OK) {
				ami_status = AMI_DIGEST_ERR;
				goto out;
			}
		} else {
			MD5Update(hContext,
			    (uint8_t *) pData, (uint32_t) dataLen);
		}
	} else {
		/*
		 * AMI_END_DATA. Determine the digest output
		 * length.
		 */
		if (__ami_cmp_oid(amih->dig_alg->algorithm,
		    AMI_MD2_OID, &equal) == AMI_OK && equal == 0)
			digestLen = AMI_MD2_DIGEST_LEN;
		else if (__ami_cmp_oid(amih->dig_alg->algorithm,
		    AMI_MD5_OID, &equal) == AMI_OK && equal == 0)
			digestLen = AMI_MD5_DIGEST_LEN;
		else if (__ami_cmp_oid(amih->dig_alg->algorithm,
		    AMI_SHA_1_OID, &equal) == AMI_OK && equal == 0)
			digestLen = AMI_SHA1_DIGEST_LEN;
		else {
			ami_status = AMI_ALGORITHM_UNKNOWN;
			goto out;
		}
		/*
		 * Check if caller provided memory of
		 * appropriate size for
		 * digest output. If no memory was provided,
		 * we have to
		 * allocate it. We have already made sure
		 * that both "ppDigest"
		 * and "pDigestLen" are set.
		 *
		 */
		if (*ppDigest) {
			if (*pDigestLen < digestLen) {
				/*
				 * Provided memory for output
				 * buffer too small
				 */
				ami_status = AMI_EBUFSIZE;
				goto out;
			}
			pDigest = *ppDigest;
		} else {
			pDigest = (CK_BYTE_PTR)
			    calloc(digestLen, sizeof (char));
			if (!pDigest) {
				ami_status = AMI_ENOMEM;
				goto out;
			}
		}

		if (equal_md5 != 0) {
			if (pData) {
				/*
				 * There are two options here,
				 * depending on whether or
				 * not amih->add_data_flag is set:
				 *
				 * 1. If amih->add_data_flag is set,
				 * we are finishing a
				 * multiple-part digest operation,
				 * so call
				 * C_DigestUpdate() followed by
				 * C_DigestFinal().
				 *
				 * 2. Otherwise, we are digesting
				 * single-part data, so
				 * call C_Digest().
				 */
				if (amih->add_data_flag) {
					if (C_DigestUpdate(hSession,
					    (uchar_t *) pData,
					    (CK_USHORT) dataLen) !=
					    CKR_OK) {
						ami_status =
						    AMI_DIGEST_ERR;
						goto out;
					}
					if (C_DigestFinal(hSession,
					    pDigest, &usDigestlen) !=
						CKR_OK) {
						ami_status =
						    AMI_DIGEST_ERR;
						goto out;
					}
				} else {
					if (C_Digest(hSession,
					    (uchar_t *) pData,
					    (CK_USHORT) dataLen,
					    pDigest,
					    &usDigestlen) !=
					    CKR_OK) {
						ami_status =
						    AMI_DIGEST_ERR;
						goto out;
					}
				}
			} else {
				/*
				 * No input data provided,
				 * so we are finishing a
				 * multiple-part digest operation
				 */
				if (C_DigestFinal(
				    hSession, pDigest, &usDigestlen) !=
				    CKR_OK) {
					ami_status = AMI_DIGEST_ERR;
					goto out;
				}
			}
		} else {
			if (pData) {
				MD5Update(hContext,
				    (uint8_t *) pData,
				    (uint32_t) dataLen);
				MD5Final(pDigest, hContext);
			} else {
				MD5Final(pDigest, hContext);
			}
		}
	}

	/* success */
	if (equal_md5 != 0) {
		if (pDigestLen)
			*pDigestLen = usDigestlen;
	} else
		*pDigestLen = digestLen;

	if (ppDigest && *ppDigest == 0)
		*ppDigest = (uchar_t *) pDigest;
	ami_status = AMI_OK;

out:
	/* Clear the session */
	if (flag == AMI_END_DATA || ami_status != AMI_OK) {
		if (hSession) {
			C_CloseSession(hSession);
			amih->hCkSession = hSession = 0;
		}
		if (hContext != NULL) {
			free(amih->hCkContext);
			amih->hCkContext = hContext = NULL;
		}
		amih->dig_alg = 0;
		amih->add_data_flag = 0;
	}
	if (ami_status != AMI_OK) {
		if (pDigest && ppDigest && pDigest != *ppDigest)
			/*
			 * We allocated memory for the
			 * output buffer
			 */
			free(pDigest);
	}
	return (ami_status);
}
