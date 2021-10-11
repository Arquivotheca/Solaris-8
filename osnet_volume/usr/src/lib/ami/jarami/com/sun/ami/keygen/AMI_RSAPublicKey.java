/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_RSAPublicKey.java	1.2 99/07/19 SMI"
 *
 */

package com.sun.ami.keygen;

import java.math.BigInteger;
import java.security.interfaces.RSAPublicKey;
import java.security.spec.RSAPublicKeySpec;
import java.security.spec.X509EncodedKeySpec;

import com.sun.ami.AMI_Exception;
import java.security.InvalidKeyException;

/**
 * This class implements the RSAPublicKey interface to
 * create an AMI_RSAPublicKey.
 *
 * @see PublicKey
 *
 */


public final class AMI_RSAPublicKey implements RSAPublicKey
{
    private BigInteger modulus;
    private BigInteger exponent;
    private byte[]     encodedKey;

    protected AMI_RSAPublicKey() {
	// Connot be instantiated this way outside the package
    }

    /**
     * This contructor takes in an RSAPublicKeySpec and creates an
     * AMI_RSAPublicKey from it, after decoding it.
     */
    public AMI_RSAPublicKey(RSAPublicKeySpec rsaKeySpec) {
	modulus = rsaKeySpec.getModulus();
	exponent = rsaKeySpec.getPublicExponent();
    }

    /**
     * This contructor takes in an AMI_KeyGen and creates an
     * AMI_RSAPublicKey from it, after decoding it.
     */
    public AMI_RSAPublicKey(AMI_KeyGen keyGen) throws InvalidKeyException {
	this(keyGen.getPublicKey());
    }

    /**
     * This contructor takes in an encoded byte[] and creates an
     * AMI_RSAPublicKey from it.
     */
    public AMI_RSAPublicKey(byte[] encoded) throws InvalidKeyException {
	encodedKey = new byte[encoded.length];
	System.arraycopy(encoded, 0, encodedKey, 0, encoded.length);
	
	AMI_KeyGen keyGen = null;
	try {
	    keyGen = new AMI_KeyGen();
	    keyGen.ami_extract_public_modexp(encodedKey);
	} catch (AMI_Exception e) {
	    throw (new InvalidKeyException());
	}
	modulus = AMI_KeyGen.toBigInt(keyGen._publicModulus);
	exponent = AMI_KeyGen.toBigInt(keyGen._publicExponent);
    }

    /**
     * This contructor takes in an X509EncodedKeySpec and creates an
     * AMI_RSAPublicKey from it, after decoding it.
     */
    public AMI_RSAPublicKey(X509EncodedKeySpec encodedKeySpec)
    throws InvalidKeyException {
	this(encodedKeySpec.getEncoded());
    }

    /**
     * Returns the name of the key generation algorithm
     *
     * @return String
     */
    public String getAlgorithm() {
        return "RSA";
    }

    /**
     * Returns the DER Encoded public key as a byte[]
     *
     * @return byte[]
     */
    public byte[] getEncoded() {
        return encodedKey;
    }

    /**
     * Returns the format of encoding of the key (BER)
     *
     * @return String
     */
    public String getFormat() {
        return "X.509";
    }

    /**
     * Returns the modulus
     *
     * @return BigInteger
     */
    public BigInteger getModulus()
    {
        return modulus;
    }

    /**
     * Returns the exponent
     *
     * @return BigInteger
     */
    public BigInteger getPublicExponent()
    {
        return exponent;
    }

    public String toString() {
        String str = getAlgorithm() + ":\nX.509:\n";
	str += (new BigInteger(encodedKey)).toString();
	return str;
    }
}
