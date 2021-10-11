/*
 * ident	"@(#)ServiceLocationEnumeration.java	1.2	99/05/10 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

//  SCCS Status:      @(#)ServiceLocationEnumeration.java	1.2	05/10/99
//  ServiceLocationEnumeration.java:
//  Author:           James Kempf
//  Created On:       Wed May 13 17:38:01 1998
//  Last Modified By: James Kempf
//  Last Modified On: Tue May 26 13:15:57 1998
//  Update Count:     5
//

package com.sun.slp;

import java.util.*;

/**
 * The ServiceLocationEnumeration interface extends Enumeration
 * with a method that can throw an exception.
 *
 * @version 1.2 00/10/14
 * @author James Kempf
 */

public interface ServiceLocationEnumeration extends Enumeration {

    Object next() throws ServiceLocationException;

}
