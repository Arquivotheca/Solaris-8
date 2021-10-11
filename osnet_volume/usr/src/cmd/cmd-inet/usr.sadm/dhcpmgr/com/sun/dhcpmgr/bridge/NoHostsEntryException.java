/*
 * @(#)NoHostsEntryException.java	1.1	99/04/12 SMI
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.bridge;

public class NoHostsEntryException extends BridgeException {
    public NoHostsEntryException() {
	super();
    }
    
    public NoHostsEntryException(String s) {
	super(s);
    }
}
