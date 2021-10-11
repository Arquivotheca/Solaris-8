/*
 * @(#)ExistsException.java	1.1	99/03/22 SMI
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.bridge;

public class ExistsException extends BridgeException {
    public ExistsException() {
	super();
    }
    
    public ExistsException(String s) {
	super(s);
    }
}
