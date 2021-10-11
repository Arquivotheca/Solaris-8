/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_Exception.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami;

public class AMI_Exception extends Exception {

	String message;

	public AMI_Exception() {
		super();
		message = null;
	}

	public AMI_Exception(String mesg) {
		super();
		message = new String(mesg);
	}

	public String toString() {
		if (message == null)
			return (super.toString());
		else
			return (message + ";" + super.toString());
	}

        public String getMessage() {
	        return message;
        }
}
