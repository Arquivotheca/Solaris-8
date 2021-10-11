/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_SignatureException.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.sign;

import com.sun.ami.AMI_Exception;

/**
 * This class provides an exception thrown during signatures.
 * 
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 * @see AMI_Exception
 *
 */

public class AMI_SignatureException extends AMI_Exception {

	public AMI_SignatureException() {
		super();
	}

	public AMI_SignatureException(String mesg) {
		super(mesg);
	}
}
