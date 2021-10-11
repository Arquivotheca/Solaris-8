/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_UsageException.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.cmd;

import com.sun.ami.AMI_Exception;

/**
 * This class provides an exception for all commands, if the 
 * command-line options are incorrect.
 * 
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 * @see AMI_Exception
 *
 */

public class AMI_UsageException extends AMI_Exception {

	public AMI_UsageException() {
		super();
	}

	public AMI_UsageException(String mesg) {
		super(mesg);
	}
}
