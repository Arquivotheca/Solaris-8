/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_KeyGenException.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.keygen;

import com.sun.ami.AMI_Exception;

/**
 * This class provides an exception thrown during key generation.
 * 
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 * @see AMI_Exception
 *
 */

public class AMI_KeyGenException extends AMI_Exception {

	public AMI_KeyGenException() {
		super();
	}

	public AMI_KeyGenException(String mesg) {
		super(mesg);
	}
}
