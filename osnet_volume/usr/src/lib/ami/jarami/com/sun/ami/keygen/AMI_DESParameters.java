/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_DESParameters.java	1.2 99/07/11 SMI"
 *
 */

package com.sun.ami.keygen;

import java.util.*;
import java.io.*;
import sun.security.util.DerInputStream;
import sun.security.util.DerOutputStream;
import java.security.AlgorithmParametersSpi;
import java.security.spec.AlgorithmParameterSpec;
import java.security.spec.InvalidParameterSpecException;
import com.sun.ami.keygen.AMI_IvParameterSpec;

/**
 * This class implements the parameter (IV) used with the DES algorithm in
 * feedback-mode. IV is defined in the standards as follows:
 * This is a almost a copy of DESParameters to provide ways to encrypt and 
 * decrypt using DES in the absence of export controlled JCE.
 *
 * <pre>
 * IV ::= OCTET STRING  -- 8 octets
 * </pre>
 *
 * @author Bhavna Bhatnagar
 *
 * @version 1.0, 02/11/99
 */

public class AMI_DESParameters extends AlgorithmParametersSpi {

    // the iv
    private byte[] iv;

    protected void engineInit(AlgorithmParameterSpec paramSpec) 
	throws InvalidParameterSpecException
    {
	if (!(paramSpec instanceof AMI_IvParameterSpec)) {
	    throw new InvalidParameterSpecException
		("Inappropriate parameter specification");
	}
	byte[] iv = ((AMI_IvParameterSpec)paramSpec).getIV();
	if (iv.length != 8) {
	    throw new InvalidParameterSpecException("IV not 8 bytes long");
	}	    
	this.iv = (byte[])iv.clone();
    }

    protected void engineInit(byte[] encoded)
	throws IOException
    {
	DerInputStream der = new DerInputStream(encoded);

	byte[] tmpIv = der.getOctetString();
	if (der.available() != 0) {
	    throw new IOException("IV parsing error: extra data");
	}
	if (tmpIv.length != 8) {
	    throw new IOException("IV not 8 bytes long");
	}
	this.iv = tmpIv;
    }

    protected void engineInit(byte[] encoded, String decodingMethod)
	throws IOException
    {
	engineInit(encoded);
    }

    protected AlgorithmParameterSpec engineGetParameterSpec(Class paramSpec)
	throws InvalidParameterSpecException
    {    
	try {
	    Class ivParamSpec = Class.forName
		("com.sun.ami.keygen.AMI_IvParameterSpec");
	    if (ivParamSpec.isAssignableFrom(paramSpec)) {
		return new AMI_IvParameterSpec(this.iv);
	    } else {
		throw new InvalidParameterSpecException
		    ("Inappropriate parameter specification");
	    }
	} catch (ClassNotFoundException e) {
	    throw new InvalidParameterSpecException
		("Unsupported parameter specification: " + e.getMessage());
	}
    }

    protected byte[] engineGetEncoded() throws IOException {
	DerOutputStream	out = new DerOutputStream();
	out.putOctetString(this.iv);
	return out.toByteArray();
    }

    protected byte[] engineGetEncoded(String encodingMethod)
	throws IOException
    {
	return engineGetEncoded();
    }

    /*
     * Returns a formatted string describing the parameters.
     */
    protected String engineToString() {
        String str = "AMI_DESParameters::";

	str += "\n\tIv = ";
	for (int ii = 0; ii < iv.length; ii ++) 
		str += iv[ii] + " ";

	str += "\n";

	return str;
    }
}
