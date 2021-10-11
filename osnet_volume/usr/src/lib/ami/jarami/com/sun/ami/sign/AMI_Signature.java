/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_Signature.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.sign;

import com.sun.ami.AMI_Exception;
import com.sun.ami.common.AMI_Common;

/**
 * This class is the base AMI RSA signature class,
 * which provides native wrapper methods
 * for all AMI-native signature methods.
 */

public class AMI_Signature extends AMI_Common {

	private byte[] _sign;

	public AMI_Signature() {
		super();
	}

	// Native method for RSA signature
	public native void ami_rsa_sign(byte[] toBeSigned, 
	    byte[] key, String sigAlg,
	    String keyAlg, int keyLen) 
	    throws AMI_SignatureException;

	// Native method for RSA signature
	public native boolean ami_rsa_verify(byte[] signature, 
	    byte[] toBeSigned, byte[] key, 
	    String sigAlg, String keyAlg) 
	    throws AMI_SignatureException;

	// Return the generated signature
	public byte[] getSign() {
		return (_sign);
	}

}

