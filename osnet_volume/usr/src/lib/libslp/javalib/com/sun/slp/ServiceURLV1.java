/*
 * ident	"@(#)ServiceURLV1.java	1.2	99/05/10 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

//  SCCS Status:      @(#)ServiceURLV1.java	1.2	05/10/99
//  ServiceURLV1.java: SLPv1 Service URL class.
//  Author:           James Kempf
//  Created On:       Fri Oct  9 19:08:53 1998
//  Last Modified By: James Kempf
//  Last Modified On: Wed Oct 14 17:00:08 1998
//  Update Count:     3
//

package com.sun.slp;

import java.util.*;
import java.io.*;

/**
 * ServiceURLV1 enforces no abstract types and no non-service: URL types for
 * SLPv1 queries.
 *
 * @version 1.2 00/10/14
 * @author James Kempf
 */

class ServiceURLV1 extends ServiceURL implements Serializable {

    ServiceURLV1(String URL, int iLifetime) throws IllegalArgumentException {
	super(URL, iLifetime);

	ServiceType serviceType = this.getServiceType();

	// Check for illegal service types.

	if (serviceType.isAbstractType()) {
	    throw
		new IllegalArgumentException(
		SLPConfig.getSLPConfig().formatMessage("v1_abstract_type",
						       new Object[0]));

	}

	if (!serviceType.isServiceURL()) {
	    throw
		new IllegalArgumentException(
			SLPConfig.getSLPConfig().formatMessage("v1_not_surl",
							       new Object[0]));

	}
    }
}
