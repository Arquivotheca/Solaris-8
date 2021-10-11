/*
 * ident	"@(#)SLPV1CDAAdvert.java	1.2	99/05/10 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

//  SCCS Status:      @(#)SLPV1CDAAdvert.java	1.2	05/10/99
//  SLPV1CDAAdvert.java: SLP V1 compatible client side DAAdvert
//  Author:           James Kempf
//  Created On:       Fri Oct  9 14:20:16 1998
//  Last Modified By: James Kempf
//  Last Modified On: Mon Nov  2 15:59:49 1998
//  Update Count:     10
//


package com.sun.slp;

import java.util.*;
import java.io.*;


/**
 * The SLPV1CDAAdvert class models the SLP V1 DAAdvert message, client side.
 *
 * @version 1.2 00/10/14
 * @author James Kempf
 */

class SLPV1CDAAdvert extends CDAAdvert {

    boolean unsolicited = true;   // we assume unsolicited, set if solicited.

    // Construct a SLPV1CDAAdvert from the byte input stream.

    SLPV1CDAAdvert(SLPHeaderV1 hdr, DataInputStream dis)
	throws ServiceLocationException, IOException {
	super(hdr, dis);

	// Super initializes it.

    }

    // Initialize object from input stream.

    protected void initialize(DataInputStream dis)
	throws ServiceLocationException, IOException {

	SLPHeaderV1 hdr = (SLPHeaderV1)getHeader();

	// Parse in error code.

	hdr.errCode = (short)hdr.getInt(dis);

	// Don't parse the rest if there's an error.

	if (hdr.errCode != ServiceLocationException.OK) {
	    return;
	}

	// Parse in DA's service URL.

	URL =
	    hdr.parseServiceURLIn(dis,
				  false,
				  ServiceLocationException.PARSE_ERROR);

	// Validate the service URL.

	ServiceType serviceType = URL.getServiceType();

	if (!serviceType.equals(Defaults.DA_SERVICE_TYPE)) {
	    throw
		new ServiceLocationException(
				ServiceLocationException.PARSE_ERROR,
				"not_right_url",
				new Object[] {URL, "DA"});

	}

	// Parse in the scope list.

	StringBuffer buf = new StringBuffer();

	hdr.getString(buf, dis);

	hdr.scopes = hdr.parseCommaSeparatedListIn(buf.toString(), true);

	// Validate the scope list.

	int i, n = hdr.scopes.size();

	for (i = 0; i < n; i++) {
	    String scope = (String)hdr.scopes.elementAt(i);

	    SLPHeaderV1.validateScope(scope);
	    hdr.scopes.setElementAt(scope.toLowerCase().trim(), i);
	}

	// If they are unscoped and we support unscoped regs, then
	//  change the scope name to default. We don't check whether we
	//  support default or not. We actually don't use these at
	//  the moment, but we still keep track of them in case we
	//  ever do reg forwarding.

	SLPConfig config = SLPConfig.getSLPConfig();

	if (config.getAcceptSLPv1UnscopedRegs() &&
	    hdr.scopes.size() == 0) {
	    hdr.scopes.addElement(Defaults.DEFAULT_SCOPE);

	}

	hdr.iNumReplies = 1;

    }

    // Can't tell if the DA is going down or not from the advert in V1,
    //  but it doesn't matter since they won't tell us anyway.

    boolean isGoingDown() {
	return false;

    }

    // Return true if the advert was unsolicited.

    boolean isUnsolicited() {
	return unsolicited;

    }

    // Set unSolicited flag.

    void setIsUnsolicited(boolean flag) {
	unsolicited = flag;

    }

}
