/*
 * ident	"@(#)SSAAdvert.java	1.2	99/05/10 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

//  SCCS Status:      @(#)SSAAdvert.java	1.2	05/10/99
//  SSAAdvert.java:   Server Side SAAdvert Message.
//  Author:           James Kempf
//  Created On:       Tue Feb 10 15:00:39 1998
//  Last Modified By: James Kempf
//  Last Modified On: Tue Oct 27 10:57:38 1998
//  Update Count:     60
//

package com.sun.slp;

import java.util.*;
import java.io.*;


/**
 * The SSAAdvert class models the SLP SAAdvert message.
 *
 * @version 1.4 97/11/20
 * @author James Kempf
 */

class SSAAdvert extends SrvLocMsgImpl {

    // Construct a SAAdvert from the arguments. This is a server side
    // for transmission to the client.

    SSAAdvert(int version,
	      short xid,
	      Locale locale,
	      ServiceURL url,
	      Vector scopes,
	      Vector attrs)
	throws ServiceLocationException {

	// Construct header.

	hdr = new SLPServerHeaderV2();

	Assert.assert(hdr != null,
		      "version_number_error",
		      new Object[] {new Integer(version)});

	hdr.functionCode = SrvLocHeader.SAAdvert;
	hdr.xid = xid;
	hdr.locale = locale;

	this.initialize(url, scopes, attrs);
    }

    // Initialize the message.

    void initialize(ServiceURL url, Vector scopes, Vector attrs)
	throws ServiceLocationException {

	SLPServerHeaderV2 hdr = (SLPServerHeaderV2)getHeader();

	ServiceType serviceType = url.getServiceType();

	if (!serviceType.equals(Defaults.SA_SERVICE_TYPE)) {
	    throw
		new ServiceLocationException(
				ServiceLocationException.PARSE_ERROR,
				"ssaadv_nonsaurl",
				new Object[] {serviceType});

	}

	// Validate scope list.

	DATable.validateScopes(scopes, hdr.locale);
	hdr.scopes = (Vector)scopes.clone();

	// Escape scope strings.

	hdr.escapeScopeStrings(scopes);

	// Parse out the payload.

	ByteArrayOutputStream baos = new ByteArrayOutputStream();

	String surl = url.toString();

	// Parse out the URL.

	byte[] urlBytes = hdr.putString(surl, baos);

	// Parse out the scope list. We need to save the bytes for
	//  the authblock.

	byte[] scopeBytes =
	    hdr.parseCommaSeparatedListOut(scopes, baos);

	// Parse out the attribute list.

	byte[] attrBytes = hdr.parseAttributeVectorOut(attrs,
						       url.getLifetime(),
						       false,
						       null,
						       baos,
						       false);

	// Parse out auth blocks, if necessary.

	Hashtable auth = null;
	byte nBlocks = 0;

	if (SLPConfig.getSLPConfig().getHasSecurity()) {
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

	    auth = hdr.getCheckedAuthBlockList(message,
					       url.getLifetime());
	    nBlocks = (byte) auth.size();

	}

	// Parse out number of blocks.

	baos.write(nBlocks);
	hdr.nbytes++;

	// Parse out blocks, if any.

	if (auth != null) {
	    AuthBlock.externalizeAll(hdr, auth, baos);

	}

	// Save bytes.

	hdr.payload = baos.toByteArray();

	// Construct description of outgoing packet for logging.

	hdr.constructDescription("SAAdvert",
				 "         URL=``" + url + "''\n" +
				 "         attrs=``" + attrs + "''\n" +
				 "         auth block="+AuthBlock.desc(auth) +
				 "\n");

    }
}
