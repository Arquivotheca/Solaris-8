/*
 * ident	"@(#)SSrvTypeMsg.java	1.2	99/05/10 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

//  SCCS Status:      @(#)SSrvTypeMsg.java	1.2	05/10/99
//  SSrvTypeMsg.java: Message class for SLP service type request
//  Author:           James Kempf
//  Created On:       Thu Oct  9 14:29:22 1997
//  Last Modified By: James Kempf
//  Last Modified On: Tue Oct 27 10:57:38 1998
//  Update Count:     101
//

package com.sun.slp;

import java.util.*;
import java.io.*;


/**
 * The SSrvTypeMsg class models the SLP service type request message.
 *
 * @version SSrvTypeMsg.java.2 00/10/14
 * @author James Kempf
 */

class SSrvTypeMsg extends SrvLocMsgImpl {

    String namingAuthority = ""; 	      // empty string is IANA

    // Construct a SSrvTypeMsg from the byte input stream. This will be
    // a SrvTypeRqst.

    SSrvTypeMsg(SrvLocHeader hdr, DataInputStream dis)
	throws ServiceLocationException, IOException {

	super(hdr, SrvLocHeader.SrvTypeRqst);

	this.initialize(dis);

    }

    // Initialize the message.

    void initialize(DataInputStream dis)
	throws ServiceLocationException, IOException {

	SLPServerHeaderV2 hdr = (SLPServerHeaderV2)getHeader();
	StringBuffer buf = new StringBuffer();

	// First get the previous responder.

	hdr.parsePreviousRespondersIn(dis);

	// Now get naming authority.

	namingAuthority = parseNamingAuthorityIn(hdr, dis, Defaults.UTF8);

	// Error if equals IANA.

	if (namingAuthority.equalsIgnoreCase(ServiceType.IANA)) {
	    throw
		new ServiceLocationException(
				ServiceLocationException.PARSE_ERROR,
				"sstm_iana",
				new Object[0]);

	}

	// Finally get scopes.

	hdr.parseScopesIn(dis);

	// Construct description.

	hdr.constructDescription("SrvTypeRqst",
				 "           naming authority=``" +
				 namingAuthority + "''\n");
    }

    // Parse a naming authority name and verify.

    protected String
	parseNamingAuthorityIn(SrvLocHeader hdr,
			       DataInputStream dis,
			       String charCode)
	throws ServiceLocationException, IOException {

	int len = 0;

	len = hdr.getInt(dis);

	// Handle the special cases of no naming authority or
	// all authorities.

	if (len == 0) {
	    return "";

	} else if (len == 0xFFFF) {	
	    return Defaults.ALL_AUTHORITIES;

	}

	byte bStr[] = new byte[len];

	dis.readFully(bStr, 0, len);
	hdr.nbytes += len;

	// Convert to string.

	String name = hdr.getBytesString(bStr, charCode).toLowerCase();

	// Validate.

	ServiceType.validateTypeComponent(name);

	return name;
    }

    // Construct a SSrvTypeMsg from the arguments. This will be a
    //  SrvTypeRply for transmission to client.

    SrvLocMsg makeReply(Vector typeNames)
	throws ServiceLocationException {

	SLPServerHeaderV2 hdr =
	    ((SLPServerHeaderV2)getHeader()).makeReplyHeader();

	hdr.iNumReplies = typeNames.size();

	// Construct payload.

	ByteArrayOutputStream baos = new ByteArrayOutputStream();

	hdr.parseCommaSeparatedListOut(typeNames, baos);

	hdr.payload = baos.toByteArray();

	// Construct description.

	hdr.constructDescription("SrvTypeRply",
				 "           types=``" + typeNames + "''\n");

	return hdr;
    }
}
