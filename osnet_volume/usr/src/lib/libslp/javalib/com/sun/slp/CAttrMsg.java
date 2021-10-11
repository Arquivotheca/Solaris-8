/*
 * ident	"@(#)CAttrMsg.java	1.2	99/05/10 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

//  SCCS Status: @(#)CAttrMsg.java	1.2 05/10/99 
//  CAttrMsg.java: Message class for SLP attribute
//                 reply.  
//  Author: James Kempf Created On: Thu Oct 9 15:17:36 1997 
//  Last Modified By: James Kempf
//  Last Modified On: Tue Oct 27 10:57:38 1998
//  Update Count: 107
//

package com.sun.slp;

import java.util.*;
import java.io.*;


/**
 * The CAttrMsg class models the SLP client side attribute message.
 *
 * @version 1.2 00/10/14
 * @author James Kempf
 */

class CAttrMsg extends SrvLocMsgImpl {

    // Vector of ServiceLocationAttribute objects
    Vector attrList = new Vector();  
    Hashtable attrAuthBlock = null;  // auth block list for objects

    // Only used for testing.

    protected CAttrMsg() { }

    // Construct a CAttrMsg from the byte input stream. 

    CAttrMsg(SLPHeaderV2 hdr, DataInputStream dis) 
	throws ServiceLocationException, IOException {

	super(hdr, SrvLocHeader.AttrRply);

	// Don't parse the rest if there's an error.

	if (hdr.errCode != ServiceLocationException.OK) {
	    return;

	}

	// Ignore if overflow.

	if (hdr.overflow) { 
	    return;

	}

	// Parse in the potentially authenticated attribute list.

	attrAuthBlock = 
	    hdr.parseAuthenticatedAttributeVectorIn(attrList, dis, true);

	// Verify authentication, if necessary.

	if (attrAuthBlock != null) {
	    AuthBlock.verifyAll(attrAuthBlock);
	}

	// Set the number of replies.

	hdr.iNumReplies = attrList.size();

    }

    // Construct a CAttrMsg payload from the arguments. This will be
    //   an AttrRqst message.

    CAttrMsg(Locale locale, ServiceURL url, Vector scopes, Vector tags) 
	throws ServiceLocationException {

	this.hdr = new SLPHeaderV2(SrvLocHeader.AttrRqst, false, locale);

	constructPayload(url.toString(), scopes, tags);

    }

    // Construct a CAttrMsg payload from the arguments. This will be
    //   an AttrRqst message.

    CAttrMsg(Locale locale, ServiceType type, Vector scopes, Vector tags) 
	throws ServiceLocationException {

	this.hdr = new SLPHeaderV2(SrvLocHeader.AttrRqst, false, locale);

	constructPayload(type.toString(), scopes, tags);

    }

    // Convert the message into bytes for the payload buffer.

    protected void constructPayload(String typeOrURL,
				    Vector scopes,
				    Vector tags) 
	throws ServiceLocationException {

	SLPHeaderV2 hdr = (SLPHeaderV2)this.hdr;
	hdr.scopes = (Vector)scopes.clone();

	// Set up previous responders.

	hdr.previousResponders = new Vector();

	ByteArrayOutputStream baos = new ByteArrayOutputStream();

	// Write out the service type or URL.

	hdr.putString(typeOrURL, baos);

	// Escape scope strings for transmission.

	hdr.escapeScopeStrings(scopes);

	// Parse out the scopes.

	hdr.parseCommaSeparatedListOut(scopes, baos);

	// Escape tags going out.

	hdr.escapeTags(tags);

	// Parse out the tags

	hdr.parseCommaSeparatedListOut(tags, baos);

	// Retrieve the configured SPI, if any
	String spi = "";
	if (SLPConfig.getSLPConfig().getHasSecurity()) {
	    LinkedList spiList = AuthBlock.getSPIList("sun.net.slp.SPIs");
	    if (spiList != null && !spiList.isEmpty()) {
		// There can be only one configured SPI for UAs
		spi = (String) spiList.getFirst();
	    }
	}

	hdr.putString(spi, baos);

	// Set payload.

	hdr.payload = baos.toByteArray();
    }

}
