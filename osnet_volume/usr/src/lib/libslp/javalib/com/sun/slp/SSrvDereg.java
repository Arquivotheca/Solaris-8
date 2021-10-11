/*
 * ident	"@(#)SSrvDereg.java	1.2	99/05/10 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

//  SCCS Status:       @(#)SSrvDereg.java	1.2	05/10/99
//  SSrvDereg.java:    Message class for SLP service deregistration request.
//  Author:           James Kempf
//  Created On:       Thu Oct  9 15:00:38 1997
//  Last Modified By: James Kempf
//  Last Modified On: Tue Oct 27 10:57:39 1998
//  Update Count:     102
//

package com.sun.slp;

import java.util.*;
import java.io.*;


/**
 * The SSrvDereg class models the server side SLP service deregistration. The
 * default class does SLPv2 deregs, but subclasses can do other versions
 * by redefining the initialize() and makeReply() messages.
 *
 * @version 1.2 00/10/14
 * @author James Kempf
 */

class SSrvDereg extends SrvLocMsgImpl {

    ServiceURL URL = null;		  // the service URL.
    Hashtable URLSignature = null;   // Authentication block.
    Vector tags = null;			  // Vector of String

    // Construct a SSrvDereg from the input stream.

    SSrvDereg(SrvLocHeader hdr, DataInputStream dis)
	throws ServiceLocationException, IOException {

	super(hdr, SrvLocHeader.SrvDereg);

	this.initialize(dis);

    }

    // Initialize the object.

    void initialize(DataInputStream dis)
	throws ServiceLocationException, IOException {

	SLPServerHeaderV2 hdr = (SLPServerHeaderV2)getHeader();
	StringBuffer buf = new StringBuffer();

	// Parse in scopes.

	hdr.parseScopesIn(dis);

	// Parse in the service URL.

	Hashtable ht = new Hashtable();

	URL =
	    hdr.parseServiceURLIn(dis,
				  ht,
				ServiceLocationException.INVALID_REGISTRATION);

	URLSignature = (Hashtable)ht.get(URL);

	// Get the tag lists.

	hdr.getString(buf, dis);

	tags = hdr.parseCommaSeparatedListIn(buf.toString(), true);

	// If no tags, then set the tags vector to null. This indicates
	//  that the service: URL needs to be deregistered.

	if (tags.size() <= 0) {
	    tags = null;

	} else {

	    // Unescape the tags.

	    hdr.unescapeTags(tags);

	}

	// Construct description.

	hdr.constructDescription("SrvDereg",
				 "         URL=``" + URL + "''\n" +
				 "         tags=``" + tags + "''\n" +
				 "         URL signature=" +
				 AuthBlock.desc(URLSignature) + "\n");

    }

    // Return a SrvAck. We ignore the existing flag, since in V2, fresh comes
    //  in. In this case, all we need to do is clone the header.

    SrvLocMsg makeReply() {

	SLPServerHeaderV2 hdr =
	    ((SLPServerHeaderV2)getHeader()).makeReplyHeader();

	// Construct description.

	hdr.constructDescription("SrvAck", "");

	return hdr;

    }

}
