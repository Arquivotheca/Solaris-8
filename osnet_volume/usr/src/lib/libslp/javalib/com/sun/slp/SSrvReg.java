/*
 * ident	"@(#)SSrvReg.java	1.2	99/05/10 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

//  SCCS Status:      @(#)SSrvReg.java	1.2	05/10/99
//  SSrvReg.java:      Message class for SLP service registration request.
//  Author:           James Kempf
//  Created On:       Thu Oct  9 14:47:48 1997
//  Last Modified By: James Kempf
//  Last Modified On: Tue Oct 27 10:49:18 1998
//  Update Count:     106
//

package com.sun.slp;

import java.util.*;
import java.io.*;


/**
 * The SSrvReg class models the server side SLP service registration. The
 * default class does SLPv2 regs, but subclasses can do other versions
 * by redefining the initialize() and makeReply() messages.
 *
 * @version 1.2 00/10/14
 * @author James Kempf
 */

class SSrvReg extends SrvLocMsgImpl {

    ServiceURL URL = null;	         // the service URL.
    String serviceType = "";	         // service type.
    Vector attrList = new Vector();        // ServiceLocationAttribute objects.
    Hashtable URLSignature = null;  // signature for URL.
    Hashtable attrSignature = null; // the signatures for the attributes.

    // Construct a SSrvReg from the input stream.

    SSrvReg(SrvLocHeader hdr, DataInputStream dis)
	throws ServiceLocationException, IOException {

	super(hdr, SrvLocHeader.SrvReg);

	this.initialize(dis);

    }

    // Initialize the object from the input stream.

    void initialize(DataInputStream dis)
	throws ServiceLocationException, IOException {

	SLPServerHeaderV2 hdr = (SLPServerHeaderV2)getHeader();
	StringBuffer buf = new StringBuffer();

	// Parse in the service URL

	Hashtable table = new Hashtable();

	URL =
	    hdr.parseServiceURLIn(dis,
				  table,
				ServiceLocationException.INVALID_REGISTRATION);

	URLSignature = (Hashtable)table.get(URL);

	// Parse in service type name.

	hdr.getString(buf, dis);

	// Validate and set URL type.

	ServiceType t = new ServiceType(buf.toString());

	if (!t.isServiceURL() && !t.equals(URL.getServiceType())) {
	    URL.setServiceType(t);

	}

	// Parse in the scope list.

	hdr.parseScopesIn(dis);

	// Parse in the attribute list.

	attrSignature =
	    hdr.parseAuthenticatedAttributeVectorIn(attrList, dis, false);

	hdr.constructDescription("SrvReg",
				 "       URL=``" +
				 URL + "''\n" +
				 "       service type=``" +
				 serviceType + "''\n" +
				 "       attribute list=``" +
				 attrList + "''\n" +
				 "       URL signature=" +
				 AuthBlock.desc(URLSignature) + "\n" +
				 "       attribute signature=" +
				 AuthBlock.desc(attrSignature) + "\n");
    }

    // Return a SrvAck. We ignore the existing flag, since in V2, fresh comes
    //  in. In this case, all we need to do is clone the header.

    SrvLocMsg makeReply(boolean existing) {

	SLPServerHeaderV2 hdr =
	    ((SLPServerHeaderV2)getHeader()).makeReplyHeader();

	// Construct description.

	hdr.constructDescription("SrvAck", "");

	return hdr;

    }
}
