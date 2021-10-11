/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_DigestException.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.digest;

import com.sun.ami.AMI_Exception;

/**
 * This class provides an exception thrown during digesting.
 * 
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 * @see AMI_Exception
 *
 */


public class AMI_DigestException extends AMI_Exception {

	public AMI_DigestException() {
		super();
	}

	public AMI_DigestException(String mesg) {
		super(mesg);
	}
}
