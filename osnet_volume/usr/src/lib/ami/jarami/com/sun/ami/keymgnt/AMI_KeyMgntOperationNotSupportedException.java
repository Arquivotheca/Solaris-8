/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_KeyMgntOperationNotSupportedException.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.keymgnt;

public class AMI_KeyMgntOperationNotSupportedException extends
    AMI_KeyMgntException {

	public AMI_KeyMgntOperationNotSupportedException() {
		super();
	}

	public AMI_KeyMgntOperationNotSupportedException(
	    String mesg) {
		super(mesg);
	}
}
