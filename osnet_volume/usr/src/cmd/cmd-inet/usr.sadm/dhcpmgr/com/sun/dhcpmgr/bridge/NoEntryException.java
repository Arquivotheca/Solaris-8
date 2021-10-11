/*
 * @(#)NoEntryException.java	1.1	99/03/22 SMI
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.bridge;

public class NoEntryException extends BridgeException {
    public NoEntryException() {
	super();
    }
    
    public NoEntryException(String s) {
	super(s);
    }
}
