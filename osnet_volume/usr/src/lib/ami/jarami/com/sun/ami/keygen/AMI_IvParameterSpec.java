/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_IvParameterSpec.java	1.2 99/07/11 SMI"
 *
 */

package com.sun.ami.keygen;

import java.security.spec.AlgorithmParameterSpec;

/**
 * This class specifies an <i>initialization vector</i> (IV). IVs are used
 * by ciphers in feedback mode, e.g., DES in CBC mode.
 * This is a copy if IvParameterSpec from JCE to provide for DES key
 * encrypt and decrypt in the absence of export controlled JCE.
 * 
 * @author Bhavna Bhatnagar
 *
 * @version 1.0 02/10/99
 */
public class AMI_IvParameterSpec implements AlgorithmParameterSpec {

    private byte[] iv;

    /**
     * Uses the bytes in <code>iv</code> as the IV.
     *
     * @param iv the buffer with the IV
     */
    public AMI_IvParameterSpec(byte[] iv) {
	this(iv, 0, iv.length);
    }

    /**
     * Uses the first <code>len</code> bytes in <code>iv</code>,
     * beginning at <code>offset</code> inclusive, as the IV.
     *
     * <p> The bytes that constitute the IV are those between
     * <code>iv[offset]</code> and <code>iv[offset+len-1]</code> inclusive.
     *
     * @param iv the buffer with the IV
     * @param offset the offset in <code>iv</code> where the IV
     * starts
     * @param len the number of IV bytes
     */
    public AMI_IvParameterSpec(byte[] iv, int offset, int len) {
	if (iv == null) {
            throw new IllegalArgumentException("IV missing");
        }
        if (iv.length - offset < len) {
            throw new IllegalArgumentException
		("IV buffer too short for given offset/length combination");
        }
	this.iv = new byte[len];
	System.arraycopy(iv, offset, this.iv, 0, len);
    }

    /**
     * Returns the initialization vector (IV).
     *
     * @return the initialization vector (IV)
     */
    public byte[] getIV() {
	return (byte[])this.iv.clone();
    }
}
