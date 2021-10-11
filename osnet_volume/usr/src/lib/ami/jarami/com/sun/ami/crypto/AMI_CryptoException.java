/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_CryptoException.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.crypto;

import com.sun.ami.AMI_Exception;

/**
 * This class provides an exception thrown during encryption or decryption
 * 
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 * @see AMI_Exception
 *
 */

public class AMI_CryptoException extends AMI_Exception {

	public AMI_CryptoException() {
		super();
	}

	public AMI_CryptoException(String mesg) {
		super(mesg);
	}
}
