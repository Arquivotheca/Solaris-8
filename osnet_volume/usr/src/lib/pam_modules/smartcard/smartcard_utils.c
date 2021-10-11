/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)smartcard_utils.c 1.10     99/07/21 SMI"

#include "smartcard_headers.h"

void 
cardSpec_init(OCF_CardSpec_t *cardSpec)
{

	/* Initialize cardSpec */
	cardSpec->flags = 0;
	cardSpec->cardhandle = 0;
	cardSpec->readername = OCF_WaitForCardSpec_AnyReader;
	cardSpec->cardname = OCF_WaitForCardSpec_AnyCard;
	cardSpec->aid = OCF_WaitForCardSpec_AnyAID;
	cardSpec->timeout.flags = 0;
	cardSpec->timeout.timeout = 0;
}

void
ocf_cleanup(OCF_ClientHandle_t clientHandle)
{

        /* deregister OCF client */
        OCF_DeregisterClient(clientHandle);
}

