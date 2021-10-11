/*
 *
 * ident	"@(#)pmGuiException.java	1.2	99/03/29 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmGuiException.java
 * 
 */

package com.sun.admin.pm.client;

import java.lang.*;

class pmGuiException extends Exception {

	String s = null;
    
	public pmGuiException(String s) {
		super(s);

	}
    
	public pmGuiException() {
		super();

	}

// XXX  use localized strings
	public String getLocalizedMessage() {
		return getMessage();

	}
}
