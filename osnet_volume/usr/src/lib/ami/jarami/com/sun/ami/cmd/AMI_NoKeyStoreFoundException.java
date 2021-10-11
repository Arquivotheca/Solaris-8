/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_NoKeyStoreFoundException.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.cmd;

import com.sun.ami.AMI_Exception;

/**
 * This class provides an exception which is thrown in case no
 * matching key store is found in AMI Server for the requested
 * data.
 * 
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 * @see AMI_Exception
 *
 */

public class AMI_NoKeyStoreFoundException extends AMI_Exception {

	public AMI_NoKeyStoreFoundException() {
		super();
	}

	public AMI_NoKeyStoreFoundException(String mesg) {
		super(mesg);
	}
}
