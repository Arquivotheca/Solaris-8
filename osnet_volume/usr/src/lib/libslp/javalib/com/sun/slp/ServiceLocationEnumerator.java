/*
 * ident	"@(#)ServiceLocationEnumerator.java	1.2	99/05/10 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

//  SCCS Status:      @(#)ServiceLocationEnumerator.java	1.2	05/10/99
//  ServiceLocationEnumerator.java: Class implementing SLP enumerator.
//  Author:           James Kempf
//  Created On:       Thu May 21 14:36:55 1998
//  Last Modified By: James Kempf
//  Last Modified On: Thu May 21 14:37:21 1998
//  Update Count:     1
//

package com.sun.slp;

import java.util.*;

/**
 * The ServiceLocationEnumerator class implements an enumeration.
 * Besides the standard Enumeration classes, it implements a next()
 * method that can (but not in this implementation) throw a
 * ServiceLocationException.
 *
 * @version 1.2 00/10/14
 * @author James Kempf
 */

class ServiceLocationEnumerator extends Object
    implements ServiceLocationEnumeration {

    // The base enumerator.

    Enumeration base;

    /**
     * The constructor simply takes an enumerator on the vector.
     */

    public ServiceLocationEnumerator(Vector v) {

	if (v != null) {
	    base = v.elements();
	} else {
	    base = (new Vector()).elements();
	}
    }

    /**
     * Pass through to the Enumerator method.
     */

    public boolean hasMoreElements() {
	return base.hasMoreElements();
    }

    /**
     * Pass through to the Enumerator method.
     */

    public Object nextElement() throws NoSuchElementException {
	return base.nextElement();
    }

    /**
     * Pass through to the Enumerator method.
     */

    public Object next() throws ServiceLocationException {
	return base.nextElement();
    }

}
