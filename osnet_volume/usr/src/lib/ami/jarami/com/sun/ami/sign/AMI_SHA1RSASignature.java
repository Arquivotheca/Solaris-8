/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_SHA1RSASignature.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.sign;

import java.security.*;
import com.sun.ami.AMI_Exception;

/**
 * Implementation for SHA1/RSA Signature
 */

public final class AMI_SHA1RSASignature extends AMI_RSASignature {

	public AMI_SHA1RSASignature() throws NoSuchAlgorithmException,
	    AMI_Exception {
		super(new String("SHA1/RSA"));
	}

}
