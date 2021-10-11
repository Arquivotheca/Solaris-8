/*
 * ident	"@(#)CSAAdvert.java	1.2	99/05/10 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

//  SCCS Status:      @(#)CSAAdvert.java	1.2	05/10/99
//  CSAAdvert.java:    Message class for SLP CSAAdvert message
//  Author:           James Kempf
//  Created On:       Fri Oct 10 10:48:05 1997
//  Last Modified By: James Kempf
//  Last Modified On: Tue Oct 27 10:57:41 1998
//  Update Count:     95
//

package com.sun.slp;

import java.util.*;
import java.io.*;


/**
 * The CSAAdvert class models the SLP SAAdvert message, client side.
 *
 * @version 1.2 00/10/14
 * @author James Kempf
 */


class CSAAdvert extends SrvLocMsgImpl {

    ServiceURL URL = null;	// The DA's service URL
    Hashtable authBlock = null;	// Scope auth blocks.
    Vector attrs = new Vector(); // The attributes.

    // Construct a CSAAdvert from the input stream.

    CSAAdvert(SLPHeaderV2 hdr, DataInputStream dis) 
	throws ServiceLocationException, IOException {
	super(hdr, SrvLocHeader.SAAdvert);

	// Parse in SA's service URL.

	StringBuffer buf = new StringBuffer();

	byte[] urlBytes = hdr.getString(buf, dis);

	try {

	    URL = new ServiceURL(buf.toString(), ServiceURL.LIFETIME_NONE);

	} catch (IllegalArgumentException ex) {

	    throw 
		new ServiceLocationException(
				ServiceLocationException.PARSE_ERROR,
				"malformed_url",
				new Object[] {ex.getMessage()});

	}

	// Validate the service URL.

	ServiceType serviceType = URL.getServiceType();

	if (!serviceType.equals(Defaults.SA_SERVICE_TYPE)) {
	    throw 
		new ServiceLocationException(
				ServiceLocationException.PARSE_ERROR, 
				"not_right_url",
				new Object[] {URL, "SA"});

	}

	// Parse in the scope list.

	byte[] scopeBytes = hdr.getString(buf, dis);
   
	hdr.scopes = 
	    hdr.parseCommaSeparatedListIn(buf.toString(), true);

	// Unescape scopes.

	hdr.unescapeScopeStrings(hdr.scopes);

	// Validate scope list.

	DATable.validateScopes(hdr.scopes, hdr.locale);

	// Parse in attributes.

	byte attrBytes[] = hdr.parseAttributeVectorIn(attrs, dis, false);

	// Construct bytes for auth.

	Object[] message = new Object[6];

	// None of the strings have leading length fields, so add them here
	ByteArrayOutputStream abaos = new ByteArrayOutputStream();
	hdr.putInteger(urlBytes.length, abaos);
	message[0] = abaos.toByteArray();
	message[1] = urlBytes;

	abaos = new ByteArrayOutputStream();
	hdr.putInteger(attrBytes.length, abaos);
	message[2] = abaos.toByteArray();
	message[3] = attrBytes;

	abaos = new ByteArrayOutputStream();
	hdr.putInteger(scopeBytes.length, abaos);
	message[4] = abaos.toByteArray();
	message[5] = scopeBytes;

	// Parse in an auth block if there.

	authBlock = hdr.parseSignatureIn(message, dis);

	// Set number of replies.

	hdr.iNumReplies = 1;

    }
}
