/*
 * @(#)BridgeException.java	1.1 99/03/22 SMI
 *
 * Copyright (c) 1998 Sun Microsystems, Inc.
 * All rights reserved.
 */

package com.sun.dhcpmgr.bridge;

public class BridgeException extends Exception {
    public BridgeException() {
	super();
    }
    
    public BridgeException(String s) {
	super(s);
    }
}
