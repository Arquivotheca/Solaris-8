/*
 * ident	"@(#)SAttrMsg.java	1.2	99/05/10 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

//  SCCS Status:      @(#)SAttrMsg.java	1.2	05/10/99
//  SAttrMsg.java:    Message class for SLP attribute request.
//  Author:           James Kempf
//  Created On:       Thu Oct  9 14:24:55 1997
//  Last Modified By: James Kempf
//  Last Modified On: Tue Oct 27 10:57:41 1998
//  Update Count:     131
//

package com.sun.slp;

import java.util.*;
import java.io.*;


/**
 * The SAttrMsg class models the SLP server side attribute message.
 * Subclasses for other versions can specialize the
 * initialize() and makeReply() methods.
 *
 * @version SAttrMsg.java.2 00/10/14
 * @author James Kempf
 */

class SAttrMsg extends SrvLocMsgImpl {

    ServiceURL URL = null;      // nonNull if a URL query.
    String serviceType = null;  // nonNull if a service type query.
    Vector tags = new Vector(); // Vector of String tags.
    String spi = "";	      // requested SPI

    protected SAttrMsg() {}

    // Construct a SAttrMsg from the input stream. This will
    //  be an SLP attribute request.

    SAttrMsg(SrvLocHeader hdr, DataInputStream dis)
	throws ServiceLocationException, IOException {

	super(hdr, SrvLocHeader.AttrRqst);

	this.initialize(dis);

    }

    // Initialize the message object.

    void initialize(DataInputStream dis)
	throws ServiceLocationException, IOException {

	SLPServerHeaderV2 hdr = (SLPServerHeaderV2)getHeader();
	StringBuffer buf = new StringBuffer();

	// Parse in the previous responder's list.

	hdr.parsePreviousRespondersIn(dis);

	// Parse in the URL or service type.

	hdr.getString(buf, dis);

	String urlOrServiceType = buf.toString();

	// Decide whether this is a service type or service URL

	try {
	    URL = new ServiceURL(urlOrServiceType, ServiceURL.LIFETIME_NONE);

	    serviceType = null;

	} catch (IllegalArgumentException ex) {

	    // Validate and remove IANA.

	    ServiceType t = new ServiceType(urlOrServiceType);

	    serviceType = t.toString();

	    URL = null;
	}

	// Parse in the scopes.

	hdr.parseScopesIn(dis);

	// Parse in the attribute tags.

	hdr.getString(buf, dis);

	tags = hdr.parseCommaSeparatedListIn(buf.toString(), true);

	// Unescape tags.

	hdr.unescapeTags(tags);

	// Get the SPI

	hdr.getString(buf, dis);

	spi = buf.toString();

	// Construct the description.

	hdr.constructDescription("AttrRqst",
				 "         " +
				 (URL != null ?
					("URL=``" + URL):
					("service type=``" + serviceType)) +
				 "''\n" +
				 "         tags=``" + tags + "''\n" +
				 "         spi=``" + spi + "''\n");
    }

    // Construct an SAttrMsg payload for reply to client. This will
    //  be an AttrRply message.

    SrvLocMsg makeReply(Vector attrs, Hashtable auth)
	throws ServiceLocationException {

	SLPServerHeaderV2 hdr =
	    ((SLPServerHeaderV2)getHeader()).makeReplyHeader();

	hdr.iNumReplies = attrs.size();

	// Select AuthBlock with requested SPI
	if (auth != null) {
	    AuthBlock selectedAuth = AuthBlock.getEquivalentAuth(spi, auth);
	    auth = null;
	    if (selectedAuth != null) {
		auth = new Hashtable();
		auth.put(spi, selectedAuth);
	    }
	}

	ByteArrayOutputStream baos = new ByteArrayOutputStream();

	hdr.parseAttributeVectorOut(attrs, 0, (auth != null),
				    auth, baos, true);

	hdr.payload = baos.toByteArray();

	// Construct description.

	hdr.constructDescription("AttrRply",
				 "        attributes=``" +
				 attrs +
				 "''\n" +
				 "        auth block=" +
				 AuthBlock.desc(auth) +
				 "\n");

	return hdr;

    }

}
