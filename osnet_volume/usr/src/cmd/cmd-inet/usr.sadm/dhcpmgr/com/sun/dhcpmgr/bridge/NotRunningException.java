/*
 * @(#)NotRunningException.java	1.1	99/04/16 SMI
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.bridge;

public class NotRunningException extends BridgeException {
    public NotRunningException() {
	super();
    }
    
    public NotRunningException(String s) {
	super(s);
    }
}
