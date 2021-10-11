/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_RC2ParameterSpec.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.keygen;

import java.security.spec.AlgorithmParameterSpec;

/**
 * This class specifies the parameters used with the
 * <a href="http://www.rsa.com/rsalabs/newfaq/q75.html"><i>RC2</i></a>
 * algorithm.
 *
 * <p> The parameters consist of an effective key size and optionally
 * an 8-byte initialization vector (IV) (only in feedback mode).
 *
 * <p> This class can be used to initialize a <code>Cipher</code> object that
 * implements the <i>RC2</i> algorithm.
 *
 * This code is copied from JCE, as it is required by the amiencrypt and
 * amidecrypt commands, which should not depend on JCE.
 *
 */
public class AMI_RC2ParameterSpec implements AlgorithmParameterSpec {

    private byte[] iv;
    private int effectiveKeyBits;

    /**
     * Constructs a parameter set for RC2 from the given effective key size
     * (in bits).
     *
     * @param effectiveKeyBits the effective key size in bits.
     */
    public AMI_RC2ParameterSpec(int effectiveKeyBits) {
	this.effectiveKeyBits = effectiveKeyBits;
    }

    /**
     * Constructs a parameter set for RC2 from the given effective key size
     * (in bits) and an 8-byte IV.
     *
     * <p> The bytes that constitute the IV are those between
     * <code>iv[0]</code> and <code>iv[7]</code> inclusive.
     *
     * @param effectiveKeyBits the effective key size in bits.
     * @param iv the buffer with the 8-byte IV.
     */
    public AMI_RC2ParameterSpec(int effectiveKeyBits, byte[] iv) {
	this(effectiveKeyBits, iv, 0);
    }

    /**
     * Constructs a parameter set for RC2 from the given effective key size
     * (in bits) and IV.
     *
     * <p> The IV is taken from <code>iv</code>, starting at
     * <code>offset</code> inclusive.
     * The bytes that constitute the IV are those between
     * <code>iv[offset]</code> and <code>iv[offset+7]</code> inclusive.
     *
     * @param effectiveKeyBits the effective key size in bits.
     * @param iv the buffer with the IV.
     * @param offset the offset in <code>iv</code> where the 8-byte IV
     * starts.
     */
    public AMI_RC2ParameterSpec(int effectiveKeyBits, byte[] iv, int offset) {
	this.effectiveKeyBits = effectiveKeyBits;
	if (iv == null) throw new IllegalArgumentException("IV missing");
	int blockSize = 8;
        if (iv.length - offset < blockSize) {
            throw new IllegalArgumentException("IV too short");
        }
	this.iv = new byte[blockSize];
	System.arraycopy(iv, offset, this.iv, 0, blockSize);
    }

    /**
     * Returns the effective key size in bits.
     *
     * @return the effective key size in bits.
     */
    public int getEffectiveKeyBits() {
	return this.effectiveKeyBits;
    }

    /**
     * Returns the IV or null if this parameter set does not contain an IV.
     *
     * @return the IV or null if this parameter set does not contain an IV.
     */
    public byte[] getIV() {
	if (this.iv != null)
		return (byte[])this.iv.clone();
	return null;
    }
}
