/*
 * ident	"@(#)SrvLocMsgImpl.java	1.2	99/05/10 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

//  SCCS Status:      @(#)SrvLocMsgImpl.java	1.2	05/10/99
//  SrvLocMsgImpl.java:  SrvLocMsg implementation.
//  Author:           James Kempf
//  Created On:       Tue Sep 15 10:06:27 1998
//  Last Modified By: James Kempf
//  Last Modified On: Sun Oct 11 17:11:13 1998
//  Update Count:     8
//

package com.sun.slp;

import java.util.*;

/**
 * The SrvLocMsgImpl class is the base class for all SLPv2 side SrvLocMsg
 * implementations.
 *
 * @version 1.2 00/10/14
 * @author James Kempf
 */

abstract class SrvLocMsgImpl extends Object implements SrvLocMsg {

    protected SrvLocHeader hdr = null;

    // For creating outgoing messages.

    SrvLocMsgImpl() {}

    // Check and set the header.

    SrvLocMsgImpl(SrvLocHeader hdr, int functionCode)
	throws ServiceLocationException {

	if (hdr.functionCode != functionCode) {
	    throw
		new ServiceLocationException(
				ServiceLocationException.NETWORK_ERROR,
				"wrong_reply_type",
				new Object[] {new Integer(hdr.functionCode)});
	}

	this.hdr = hdr;

    }

    // Return the header.

    public SrvLocHeader getHeader() {
	return hdr;
    }

    // Return the error code, via the header.

    public short getErrorCode() {
	return hdr.errCode;
    }

}
