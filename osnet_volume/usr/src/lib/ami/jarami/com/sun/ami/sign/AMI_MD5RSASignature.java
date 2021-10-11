/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_MD5RSASignature.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.sign;

import java.security.*;
import com.sun.ami.AMI_Exception;

/**
 * Implementation for MD5/RSA Signature
 */

public final class AMI_MD5RSASignature extends AMI_RSASignature {

	public AMI_MD5RSASignature() throws NoSuchAlgorithmException,
	    AMI_Exception {
		super(new String("MD5/RSA"));
	}

}
