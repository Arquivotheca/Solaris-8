/*
 * ident	"@(#)SrvLocMsg.java	1.2	99/05/10 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

//  SCCS Status:      @(#)SrvLocMsg.java	1.2	05/10/99
//  SrvLocMsg.java:   Abstract class for all SLP message objects.
//  Author:           James Kempf
//  Created On:       Mon Sep 14 13:03:22 1998
//  Last Modified By: James Kempf
//  Last Modified On: Tue Sep 15 09:51:56 1998
//  Update Count:     4
//

package com.sun.slp;

import java.util.*;
import java.net.*;
import java.io.*;

/**
 * SrvLocMsg is an interface supported by all SLP message objects,
 * regardless of type.
 *
 * @version 1.2 00/10/14
 * @author James Kempf
 */

interface SrvLocMsg {

    // Return the header object.

    abstract SrvLocHeader getHeader();

    // Return the error code.

    abstract short getErrorCode();

}
