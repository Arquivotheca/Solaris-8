/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_CertReqException.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.ca;

import com.sun.ami.AMI_Exception;

/**
 * This class provides an exception thrown during certreq 
 * generation or parsing.
 * 
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 * @see AMI_Exception
 *
 */


public class AMI_CertReqException extends AMI_Exception {

	public AMI_CertReqException() {
		super();
	}

	public AMI_CertReqException(String mesg) {
		super(mesg);
	}
}
