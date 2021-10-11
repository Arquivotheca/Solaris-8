/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_MD2RSASignature.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.sign;

import java.security.*;
import com.sun.ami.AMI_Exception;

/**
 * Implementation for MD2/RSA Signature
 */

public final class AMI_MD2RSASignature extends AMI_RSASignature {

	public AMI_MD2RSASignature() throws NoSuchAlgorithmException,
	    AMI_Exception {
		super(new String("MD2/RSA"));
	}

}
