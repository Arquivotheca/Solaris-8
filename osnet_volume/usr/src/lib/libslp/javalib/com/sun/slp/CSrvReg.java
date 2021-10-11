/*
 * ident	"@(#)CSrvReg.java	1.2	99/05/10 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

//  SCCS Status:      @(#)CSrvReg.java	1.2	05/10/99
//  CSrvReg.java:    Service Registration, Client Side.
//  Author:           James Kempf
//  Created On:       Tue Feb 10 12:15:43 1998
//  Last Modified By: James Kempf
//  Last Modified On: Tue Oct 27 10:57:38 1998
//  Update Count:     49
//

package com.sun.slp;

import java.util.*;
import java.io.*;


/**
 * The CSrvReg class models the client side SLP service registration 
 * message. 
 *
 * @version 1.2 00/10/14
 * @author James Kempf
 */

class CSrvReg extends SrvLocMsgImpl {

    ServiceURL URL;

    // Construct a CSrvReg from the arguments. This is the SrvReg for

    CSrvReg(boolean fresh,
	    Locale locale,
	    ServiceURL urlEntry,
	    Vector scopes,
	    Vector attrs,
	    Hashtable URLSignatures,
	    Hashtable attrSignatures) 
	throws ServiceLocationException {

	this.URL = urlEntry;

	// We do heavy checking of attributes here so that any registrations
	//  are correct.

	Hashtable attrHash = new Hashtable();
	int i, n = attrs.size();

	// Verify each attribute, merging duplicates in the vector
	//  and throwing an error if any duplicates have mismatched types.

	Vector attrList = new Vector();

	for (i = 0; i < n; i++) {
	    Object o = attrs.elementAt(i);

	    if (!(o instanceof ServiceLocationAttribute)) {
		throw
		    new IllegalArgumentException(
		SLPConfig.getSLPConfig().formatMessage("not_an_attribute",
						       new Object[0]));
	    }

	    // Make a new copy of the attribute, so we can modify it.

	    ServiceLocationAttribute attr = (ServiceLocationAttribute)o;

	    ServiceLocationAttribute.mergeDuplicateAttributes(
		new ServiceLocationAttribute(attr.getId(), attr.getValues()),
		attrHash,
		attrList,
		false);
	}

	this.initialize(fresh,
			locale,
			urlEntry,
			scopes,
			attrList,
			URLSignatures,
			attrSignatures);

    }

    // Initialize the object. V1 will do it differently.

    void initialize(boolean fresh,
		    Locale locale,
		    ServiceURL urlEntry,
		    Vector scopes,
		    Vector attrs,
		    Hashtable URLSignatures,
		    Hashtable attrSignatures) 
	throws ServiceLocationException {

	SLPConfig config = SLPConfig.getSLPConfig();
	SLPHeaderV2 hdr = new SLPHeaderV2(SrvLocHeader.SrvReg, fresh, locale);
	this.hdr = hdr;
	hdr.scopes = (Vector)scopes.clone();

	ByteArrayOutputStream baos = new ByteArrayOutputStream();

	// Parse out the URL. Ignore overflow.

	hdr.parseServiceURLOut(urlEntry,
			       config.getHasSecurity(),
			       URLSignatures,
			       baos,
			       false);

	// Parse out service type. It may be different from the
	//  service URL.

	ServiceType serviceType = urlEntry.getServiceType();

	hdr.putString(serviceType.toString(), baos);

	// Escape scope strings.

	hdr.escapeScopeStrings(scopes);

	// Parse out the scope list.

	hdr.parseCommaSeparatedListOut(scopes, baos);

	// Parse out the attribute list.

	hdr.parseAttributeVectorOut(attrs,
				    urlEntry.getLifetime(),
				    config.getHasSecurity(),
				    attrSignatures,
				    baos,
				    true);

	hdr.payload = baos.toByteArray();

    }

}
