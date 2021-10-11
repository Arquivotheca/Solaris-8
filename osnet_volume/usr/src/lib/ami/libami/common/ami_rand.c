/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ami_rand.c 1.1	99/07/11 SMI"

#include <stdlib.h>
/* #include "ami_prot.h" */
#include "ami_local.h"
#include "ami.h"
#include "ami_proto.h"
#include "cryptoki.h"

/*
 * Generate random bytes
 */

AMI_STATUS
ami_random(
    const ushort_t usRandomLen,	/* IN:  requested # of bytes */
    uchar_t ** ppRandomData	/* OUT: random byte buffer */
)
{
	CK_SESSION_HANDLE hSession = 0;
	CK_BYTE_PTR pRandomData = NULL;
	CK_RV rv;
	AMI_STATUS ami_status = AMI_EPARM;

	if (!usRandomLen || !ppRandomData)
		return (AMI_EPARM);
	/*
	 * Check if caller provided memory for the random bytes.
	 * If no memory was provided, we have to allocate it.
	 */
	if (*ppRandomData != 0)
		pRandomData = *ppRandomData;
	else {
		pRandomData = (CK_BYTE_PTR)
		    calloc(usRandomLen, sizeof (CK_BYTE));
		if (!pRandomData) {
			ami_status = AMI_ENOMEM;
			goto out;
		}
	}

	rv = C_OpenSession((CK_SLOT_ID) 0,
	    (CK_FLAGS) 0,
	    (CK_VOID_PTR) 0,
	    nullNotify,
	    &hSession);
	if (rv != CKR_OK) {
		ami_status = AMI_SYSTEM_ERR;
		goto out;
	}
	rv = C_GenerateRandom(hSession,
	    pRandomData,
	    (CK_USHORT) usRandomLen);
	if (rv != CKR_OK) {
		ami_status = AMI_RANDOM_NUM_ERR;
		goto out;
	}

	/* success */
	if (*ppRandomData == 0)
		*ppRandomData = (uchar_t *) pRandomData;
	ami_status = AMI_OK;

out:
	if (hSession)
		C_CloseSession(hSession);
	if (ami_status != AMI_OK) {
		if (pRandomData && pRandomData != *ppRandomData)
			/* We allocated memory for the output buffer */
			free(pRandomData);
	}
	return (ami_status);
}
