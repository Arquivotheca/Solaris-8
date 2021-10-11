/*
 * ident	"@(#)URLAttributeVerifier.java	1.2	99/05/10 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

//  SCCS Status:      @(#)URLAttributeVerifier.java	1.2	05/10/99
//  URLAttributeVerifier.java: Parse a service template from a URL
//  Author:           James Kempf
//  Created On:       Mon Jun 23 11:52:04 1997
//  Last Modified By: James Kempf
//  Last Modified On: Thu Jun 11 13:24:03 1998
//  Update Count:     22
//

package com.sun.slp;

import java.util.*;
import java.net.*;
import java.io.*;

/**
 * A URLAttributeVerifier object performs service template parsing from
 * a URL. Most of the work is done by the superclass. This class
 * takes care of opening the Reader on the URL.
 *
 * @version 1.2 00/10/14
 * @author James Kempf
 *
 */

class URLAttributeVerifier extends AttributeVerifier {

    /**
     * Construct a URLAttributeVerifier for the file named in the parameter.
     *
     * @param url URL from which to read the template
     * @exception ServiceLocationException Error code may be:
     *				       SYSTEM_ERROR
     *					   when the URL can't be opened or
     *					   some other i/o error occurs.
     *					PARSE_ERROR
     *					    if an error occurs during
     *					    attribute parsing.
     */

    URLAttributeVerifier(String url)
	throws ServiceLocationException {

	super();

	initialize(url);

    }

    // Open a reader on the URL and initialize the attribute verifier.

    private void initialize(String urlName)
	throws ServiceLocationException {

	InputStream is = null;

	try {

	    // Open the URL.

	    URL url = new URL(urlName);

	    // Open an input stream on the URL.

	    is = url.openStream();

	    // Initialize the verifier, by parsing the file.

	    super.initialize(new InputStreamReader(is));

	} catch (MalformedURLException ex) {

	    throw
		new ServiceLocationException(
				ServiceLocationException.INTERNAL_SYSTEM_ERROR,
				"invalid_url",
				new Object[] {urlName});

	} catch (IOException ex) {

	    throw
	  new ServiceLocationException(
				ServiceLocationException.INTERNAL_SYSTEM_ERROR,
				"url_ioexception",
				new Object[] { urlName, ex.getMessage()});

	}

	try {

	    is.close();

	} catch (IOException ex) {

	}

    }
}
