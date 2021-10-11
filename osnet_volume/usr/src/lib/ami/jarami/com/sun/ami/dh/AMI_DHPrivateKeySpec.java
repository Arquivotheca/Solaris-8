/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_DHPrivateKeySpec.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.dh;

import java.math.BigInteger;
import java.security.spec.KeySpec;

/**
 * This class specifies a Diffie-Hellman private key with its associated
 * parameters.
 *
 */

public class AMI_DHPrivateKeySpec implements KeySpec {

    // The private value
    private BigInteger x;

    // The prime modulus
    private BigInteger p;

    // The base generator
    private BigInteger g;

    // The private-value length
    private int l = 0;

    public AMI_DHPrivateKeySpec(BigInteger x, BigInteger p, BigInteger g) {
	this.x = x;
	this.p = p;
	this.g = g;
    }

    public AMI_DHPrivateKeySpec(BigInteger x, BigInteger p,
	BigInteger g, int l) {
	this.x = x;
	this.p = p;
	this.g = g;
	this.l = l;
    }

    /**
     * Returns the private value <code>x</code>.
     *
     * @return the private value <code>x</code>
     */
    public BigInteger getX() {
	return this.x;
    }

    /**
     * Returns the prime modulus <code>p</code>.
     *
     * @return the prime modulus <code>p</code>
     */
    public BigInteger getP() {
	return this.p;
    }

    /**
     * Returns the base generator <code>g</code>.
     *
     * @return the base generator <code>g</code>
     */
    public BigInteger getG() {
	return this.g;
    }

    /**
     * Returns the private-value length <code>l</code>.
     *
     * @return the private-value length <code>l</code>, or null if
     * <code>l</code> has not been set
     */
    public int getL() {
	return this.l;
    }
}
