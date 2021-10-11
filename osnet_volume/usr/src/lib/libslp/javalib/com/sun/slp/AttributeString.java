/*
 * ident	"@(#)AttributeString.java	1.2	99/05/10 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

//  SCCS Status:      @(#)AttributeString.java	1.2	05/10/99
//  AttributeString.java: Model an Attribute value string.
//  Author:           James Kempf
//  Created On:       Wed Apr  8 10:40:03 1998
//  Last Modified By: James Kempf
//  Last Modified On: Wed Jul 29 15:21:32 1998
//  Update Count:     16
//

package com.sun.slp;

import java.util.*;
import java.io.*;

/**
 * The AttributeString class embodies the SLP lower cased, space compressed
 * string matching rules. It precomputes an 
 * efficient string for matching, squeezing out whitespace and lower casing. 
 * The toString() method returns the original string. Note that it does
 * NOT handle pattern wildcard matching. 
 *
 * @version 1.2 00/10/14
 * @author James Kempf
 */

class AttributeString extends Object {

    String string;		// the original string.
    String cstring;		// whitespace separated, lower cased parts.
    Locale locale;		// the locale in which this string was created.

    // Create an attribute string. Use the passed in locale to determine
    //  the lower casing rules.

    AttributeString(String str, Locale nlocale) {

	string = str;
	locale = nlocale;

	cstring = parse(str, nlocale);

    }

    // Parse the string into whitespace separated, lower cased parts.

    private String parse(String str, Locale nlocale) {

	StringBuffer buf = new StringBuffer();

	StringTokenizer tk = 
	    new StringTokenizer(str, ServiceLocationAttribute.WHITESPACE);

	while (tk.hasMoreTokens()) {
	    buf.append(tk.nextToken().toLowerCase(nlocale));
	    buf.append(ServiceLocationAttribute.SPACE);

	}

	return buf.toString().trim();
    }

    //
    // Comparison operations.
    //

    // For compatibility with AttributePattern.

    public boolean match(AttributeString str) {
	return equals(str);

    }

    public boolean lessEqual(AttributeString str) {

	return (cstring.compareTo(str.cstring) <= 0);

    }

    public boolean greaterEqual(AttributeString str) {
	return (cstring.compareTo(str.cstring) >= 0);

    }

    //
    // Object overrides.
    //

    /**
     * Return true if obj pattern matches with this string.
     */

    public boolean equals(Object obj) {

	if (obj == this) {
	    return true;

	}

	if (!(obj instanceof AttributeString)) {
	    return false;

	}

	return cstring.equals(((AttributeString)obj).cstring);
    }

    /**
     * Return the original string.
     */

    public String toString() {
	return string;

    }

    /**
     * Hash on the computed string.
     */

    public int hashCode() {
	return cstring.toString().hashCode();

    }

}
