/*
 * ident	"@(#)SLPV1SSrvDereg.java	1.2	99/05/10 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

//  SCCS Status:      @(#)SLPV1SSrvDereg.java	2.9	02/10/98
//  SLPV1SSrvDereg.java:    Message class for SLP service deregistration
//			    request.
//  Author:           James Kempf
//  Created On:       Thu Oct  9 15:00:38 1997
//  Last Modified By: James Kempf
//  Last Modified On: Mon Jan  4 15:26:33 1999
//  Update Count:     82
//

package com.sun.slp;

import java.util.*;
import java.io.*;


/**
 * The SLPV1SSrvDereg class models the server side SLP service
 * deregistration.
 *
 * @version 1.4 97/11/20
 * @author James Kempf
 */


class SLPV1SSrvDereg extends SSrvDereg {

    // Construct a SLPV1SSrvDereg from the byte input stream.

    SLPV1SSrvDereg(SrvLocHeader hdr, DataInputStream dis)
	throws ServiceLocationException, IOException {

	super(hdr, dis);

    }

    // Initialize the object.

    void initialize(DataInputStream dis)
	throws ServiceLocationException, IOException {

	SLPHeaderV1 hdr = (SLPHeaderV1)getHeader();
	StringBuffer buf = new StringBuffer();

	// Parse in the service URL.

	URL =
	    hdr.parseServiceURLIn(dis,
				  false,
				ServiceLocationException.INVALID_REGISTRATION);

	hdr.getString(buf, dis);

	tags = hdr.parseCommaSeparatedListIn(buf.toString().trim(), true);

	// Error if any tags are wildcarded. Only allowed for AttrRqst.

	int i, n = tags.size();

	for (i = 0; i < n; i++) {
	    String tag = (String)tags.elementAt(i);

	    // Unescape tag.

	    tag =
		ServiceLocationAttributeV1.unescapeAttributeString(tag,
								hdr.charCode);
	
	    if (tag.startsWith("*") || tag.endsWith("*")) {
		throw
		    new ServiceLocationException(
				ServiceLocationException.PARSE_ERROR,
				"v1_dereg_wildcard",
				new Object[0]);
	    }

	    tags.setElementAt(tag, i);
	}

	// If no tags, then set the tags vector to null. This indicates
	//  that the service: URL needs to be deregistered.

	if (tags.size() <= 0) {
	    tags = null;
	}

	// We need to find all the scopes for this guy and put them into the
	//  scope list on the header.

	ServiceTable table = ServiceTable.getServiceTable();

	ServiceStore.ServiceRecord rec = table.getServiceRecord(URL,
								hdr.locale);

	// If the record is there, then get the scopes.

	if (rec != null) {
	    hdr.scopes = (Vector)rec.getScopes().clone();

	} else {

	    SLPConfig config = SLPConfig.getSLPConfig();

	    // We simply put in the useScopes, just to make the request.
	    //  The request will be rejected when presented to ServiceTable.

	    hdr.scopes = (Vector)config.getSAConfiguredScopes().clone();

	}

	hdr.constructDescription("SrvDereg",
				 "         URL=``" + URL + "''\n" +
				 "         tags=``" + tags + "''\n");

    }

    // Return a SrvAck.

    SrvLocMsg makeReply() {

	SLPHeaderV1 hdr = ((SLPHeaderV1)getHeader()).makeReplyHeader();

	// Construct description.

	hdr.constructDescription("SrvAck", "");

	return hdr;

    }
}
