/*
* Copyright(c) 1999 by Sun Microsystems, Inc.
* All rights reserved.
* #pragma ident "@(#)AMI_RSAPrivateKey.java	1.3 99/07/19 SMI"
*
*/

package com.sun.ami.keygen;

import java.math.BigInteger;
import java.security.interfaces.RSAPrivateKey;
import java.security.interfaces.RSAPrivateCrtKey;
import java.security.spec.RSAPrivateKeySpec;
import java.security.spec.RSAPrivateCrtKeySpec;
import java.security.spec.PKCS8EncodedKeySpec;

import com.sun.ami.AMI_Exception;
import java.security.InvalidKeyException;

/**
 * This class implements the PrivateKey interface to create an RSAPrivateKey.
 *
 * @see PrivateKey
 */


public final class AMI_RSAPrivateKey implements RSAPrivateCrtKey
{
    private final BigInteger _version = BigInteger.valueOf(0);

    private byte[]     encodedKey;
    private BigInteger modulus;
    private BigInteger exponent;
    private BigInteger pubExponent;
    private BigInteger primeP;
    private BigInteger primeQ;
    private BigInteger primeExponentP;
    private BigInteger primeExponentQ;
    private BigInteger coefficient;

    protected AMI_RSAPrivateKey() {
	// Cannot be instantiated from outside the package
    }

    public AMI_RSAPrivateKey(AMI_KeyGen keyGen) throws InvalidKeyException {
	this(keyGen.getPrivateKey());
    }

    public AMI_RSAPrivateKey(PKCS8EncodedKeySpec pkcs)
    throws InvalidKeyException {
	this (pkcs.getEncoded());
    }

    public AMI_RSAPrivateKey(RSAPrivateKeySpec keySpec)
    throws InvalidKeyException {
	this (keySpec.getModulus(), keySpec.getPrivateExponent());
    }

    public AMI_RSAPrivateKey(RSAPrivateCrtKeySpec keySpec)
    throws InvalidKeyException {
	this (keySpec.getModulus(),
	    keySpec.getPrivateExponent(),
	    keySpec.getPublicExponent(),
	    keySpec.getPrimeP(),
	    keySpec.getPrimeQ(),
	    keySpec.getPrimeExponentP(),
	    keySpec.getPrimeExponentQ(),
	    keySpec.getCrtCoefficient());
    }

    /**
     * This contructor takes in an encoded byte[] and creates an
     * AMI_RSAPrivateKey from it.
     */
    public AMI_RSAPrivateKey(byte[] encoded) throws InvalidKeyException {
	// Copy PKCS#8 encoded key
	encodedKey = new byte[encoded.length];
	System.arraycopy(encoded, 0, encodedKey, 0, encoded.length);

	// Obtain the modulus and exponent
	AMI_KeyGen keyGen = null;
	try {
	    keyGen = new AMI_KeyGen();
	    keyGen.ami_extract_private_modexp(encodedKey);
	} catch (AMI_Exception e) {
	    throw (new InvalidKeyException());
	}
	modulus = AMI_KeyGen.toBigInt(keyGen._privateModulus);
	exponent = AMI_KeyGen.toBigInt(keyGen._privateExponent);
	pubExponent = AMI_KeyGen.toBigInt(keyGen._pubExponent);
	primeP = AMI_KeyGen.toBigInt(keyGen._prime1);
	primeQ = AMI_KeyGen.toBigInt(keyGen._prime2);
	primeExponentP = AMI_KeyGen.toBigInt(keyGen._primeExponent1);
	primeExponentQ = AMI_KeyGen.toBigInt(keyGen._primeExponent2);
	coefficient = AMI_KeyGen.toBigInt(keyGen._coefficient);
    }

    /**
     * This contructor creates an AMI_RSAPrivateKey from
     * modulus and exponent
     */
    public AMI_RSAPrivateKey(BigInteger mod, BigInteger exp)
    throws InvalidKeyException {
	modulus = new BigInteger(mod.toByteArray());
	exponent = new BigInteger(exp.toByteArray());
	// Add means to obtain PKCS#8 from modulus and exponent
    }

    /**
     * This contructor creates an AMI_RSAPrivateKey from
     * CRT parameters
     */
    public AMI_RSAPrivateKey(BigInteger mod, BigInteger exp,
    BigInteger pubExp,
    BigInteger prime1, BigInteger prime2, BigInteger pexp1,
    BigInteger pexp2, BigInteger coeff)
    throws InvalidKeyException {
	modulus = new BigInteger(mod.toByteArray());
	exponent = new BigInteger(exp.toByteArray());
	pubExponent = new BigInteger(pubExp.toByteArray());
	primeP = new BigInteger(prime1.toByteArray());
	primeQ = new BigInteger(prime2.toByteArray());
	primeExponentP = new BigInteger(pexp1.toByteArray());
	primeExponentQ = new BigInteger(pexp2.toByteArray());
	coefficient = new BigInteger(coeff.toByteArray());
	// Add means to obtain PKCS#8 from modulus and exponent
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
     * Returns the BER Encoded private key as a byte[]
     *
     * @return byte[]
     */
    public byte[] getEncoded() {
	// Make a copy and return
	byte[] copy = new byte[encodedKey.length];
	System.arraycopy(encodedKey, 0, copy, 0, encodedKey.length);
        return copy;
    }

    /**
     * Returns the format of encoding of the key (DER)
     *
     * @return String
     */
    public String getFormat()
    {
        return "PKCS#8";
    }

    /**
     * Returns the raw (un-encoded key).
     * It is acceptable to return this, as the methods which use the keys
     * are only executed by the servers.
     *
     * @return byte[] representaion of the key
     */
    public byte[] getX() {
	// %%% Might have ANS.1 decode the PKCS#8 key
	// and return PKCS#1 key
        return (getEncoded());
    }

    /**
     * Returns modulus
     */
    public BigInteger getModulus() {
	return (modulus);
    }

    /**
     * Returns private exponent
     */
    public BigInteger getPrivateExponent()
    {
	return (exponent);
    }

    /**
     * Returns public exponent
     */
    public BigInteger getPublicExponent()
    {
	return (pubExponent);
    }

    /**
     * Returns primeP
     */
    public BigInteger getPrimeP()
    {
	return (primeP);
    }

    /**
     * Returns primeQ
     */
    public BigInteger getPrimeQ()
    {
	return (primeQ);
    }

    /**
     * Returns primeExponnetP
     */
    public BigInteger getPrimeExponentP()
    {
	return (primeExponentP);
    }

    /**
     * Returns primeExponentQ
     */
    public BigInteger getPrimeExponentQ()
    {
	return (primeExponentQ);
    }

    /**
     * Returns crtCoefficient
     */
    public BigInteger getCrtCoefficient()
    {
	return (coefficient);
    }

    public String toString()
    {
        String str = getAlgorithm() + ":\nPKCS#8:\n";
	str += (new BigInteger(encodedKey)).toString();
	return str;
    }
}
