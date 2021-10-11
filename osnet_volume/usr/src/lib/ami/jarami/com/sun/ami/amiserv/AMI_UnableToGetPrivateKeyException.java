/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_UnableToGetPrivateKeyException.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.amiserv;

import com.sun.ami.AMI_Exception;

/**
 * This class provides an exception which is thrown if
 * the AMI server is unable to retrieve a private key for
 * the user/host for any reason.
 * 
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 * @see AMI_Exception
 *
 */

public class AMI_UnableToGetPrivateKeyException extends AMI_Exception {

	public AMI_UnableToGetPrivateKeyException() {
		super();
	}

	public AMI_UnableToGetPrivateKeyException(String mesg) {
		super(mesg);
	}
}
