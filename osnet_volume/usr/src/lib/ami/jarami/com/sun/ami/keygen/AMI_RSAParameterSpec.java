/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_RSAParameterSpec.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.keygen;

import java.security.spec.AlgorithmParameterSpec;
import java.math.BigInteger;

/**
 * This class specifies the set of parameters used with the RSA algorithm.
 * 
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 * @see AlgorithmParameterSpec
 * @see AMI_RSAParams
 *
 */

public class AMI_RSAParameterSpec implements 
			AlgorithmParameterSpec, AMI_RSAParams
{

   // Standard acceptable exponents
    private static final int PUB_EXP_F0 = 0x03;
    private static final int PUB_EXP_F4 = 0x000101; // default exponent

    private static final int DEFAULT_KEY_SIZE = 512;

    /**
     * Creates a new AMI_RSAParameterSpec with the default parameters.
     */
    public AMI_RSAParameterSpec() {
        this(DEFAULT_KEY_SIZE, BigInteger.valueOf(PUB_EXP_F4));
    }

    /**
     * Creates a new AMI_RSAParameterSpec with the specified key size.
     * 
     * @param m the modulus.
     * 
     */
    public AMI_RSAParameterSpec(int m) {
        this(m, BigInteger.valueOf(PUB_EXP_F4));
    }

    /**
     * Creates a new AMI_RSAParameterSpec with the specified parameter values.
     * 
     * @param m the modulus size
     * 
     * @param e the exponent.
     * 
     */
    public AMI_RSAParameterSpec(int m, BigInteger e) {
	_m = m;
	_e = e;
	// The length to be sent to the SKI function is the length 
	// of the hex representaion in bytes.
	_len = (e.toString(16).getBytes()).length; 
    }

    /**
     * Returns the modulus size <code>m</code>.
     *
     * @return the modulus size <code>m</code>.
     */
    public int getM() {
	return _m;
    }

    /**
     * Returns the exponent <code>e</code>.
     *
     * @return the exponent <code>e</code>.
     */
    public BigInteger getE() {
	return _e;
    }

    /**
     * Returns the length <code>len</code> of the hex 
     * representaion of the exponent.
     *
     * @return the exponent <code>len</code>.
     */
    public int getLen() {
	return _len;
    }

    public String toString() {

	String str = "AMI_RSAParameterSpec : " + "\n";

	str += "[" + "modulus size - " + _m + "\n";
	str +=  "exponent - " + _e + "\n";
	str +=  "length - " + _len + "]\n";

	return str;
    }

    private int _m;
    private BigInteger _e;
    private int _len;
}
