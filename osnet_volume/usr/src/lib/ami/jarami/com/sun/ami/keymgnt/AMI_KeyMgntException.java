/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_KeyMgntException.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.keymgnt;

import com.sun.ami.AMI_Exception;

public class AMI_KeyMgntException extends AMI_Exception {

	public AMI_KeyMgntException() {
		super();
	}

	public AMI_KeyMgntException(String mesg) {
		super(mesg);
	}
}
