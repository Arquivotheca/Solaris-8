/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_RC4Key.java	1.2 99/07/11 SMI"
 *
 */

package com.sun.ami.keygen;

import java.math.BigInteger;
import javax.crypto.SecretKey;
import java.security.InvalidKeyException;

/**
 * This class represents a RC4 key.
 *
 * @author Sangeeta Varma
 *
 * @version 1.0
 */

public class AMI_RC4Key implements SecretKey {

    /**
     * Creates a RC4 key from a given key.
     *
     * @param key the given key
     *  
     */
    public AMI_RC4Key(BigInteger key) {
        _key = key;
    }

    public byte[] getEncoded() {
	/*
	 * Return a copy of the key, rather than a reference,
	 * so that the key data cannot be modified from outside
	 */
        byte[] copy = new byte[_key.toByteArray().length];
        System.arraycopy(_key.toByteArray(), 0, copy, 0,
			_key.toByteArray().length);
        return copy;
    }

    public String getAlgorithm() {
	return "RC4";
    }
	    
    public String getFormat() {
	return "RAW";
    }


    public String toString() {

        byte[] output = _key.toByteArray();
        String str = getAlgorithm() + ":\n  Key:\n";
	
	for (int ii = 0; ii < output.length; ii++)
	  str += output[ii] + " ";

	return str;
    }

    private BigInteger _key;
}

