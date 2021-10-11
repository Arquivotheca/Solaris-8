/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_RC2Key.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.keygen;

import java.math.BigInteger;
import javax.crypto.SecretKey;
import java.security.InvalidKeyException;

/**
 * This class represents a RC2 key.
 *
 * @author Sangeeta Varma
 *
 * @version 1.0
 */

public class AMI_RC2Key implements SecretKey {

    protected static final int RC2_KEY_SIZE = 8;

    /**
     * Creates a RC2 key from a given key
     *
     * @param key the given key
     *
     */
    public AMI_RC2Key(BigInteger key)  {
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
	return "RC2";
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

