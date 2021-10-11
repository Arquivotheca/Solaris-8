/*
 * @(#)ValidationException.java	1.1 99/03/22 SMI
 *
 * Copyright (c) 1998-1999 Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.data;

public class ValidationException extends Exception {
    public ValidationException() {
	super();
    }
    
    public ValidationException(String s) {
	super(s);
    }
}
