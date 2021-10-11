/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_KeyGen.java	1.3 99/07/19 SMI"
 *
 */


package com.sun.ami.keygen;

import java.security.*;
import java.lang.System;
import java.math.BigInteger;

import com.sun.ami.AMI_Exception;
import com.sun.ami.common.AMI_Common;


/**
 * This class is the base AMI Key generation class, which provides native 
 * wrapper methods for all AMI-native key generation algorithms.
 *
 * <P>It implements wrappers for : <UL>
 *  <LI>        RSA Key generation 
 *  <LI>        RC2 Key generation
 *  <LI>        RC4 Key generation
 *  <LI>        DES Key generation
 *  <LI>        3DES Key generation
 * 
 * @author Sangeeta Varma
 * Bhavna Bhatnagar added DES, 3DES
 * 
 * @version 1.0
 *
 * @see AMI_Common
 *
 */

public class AMI_KeyGen extends AMI_Common {
  
    /* 
     * Default Constructor.
     */ 
    public AMI_KeyGen() throws AMI_Exception {
	super();
    }

    /**
     * Native method for generating RSA keys.
     * 
     * @param m Modulus
     * @param e Exponent
     * @param len length of the exponent 
     * @param seed Random seed for producing random numbers
     * 
     */
    public native void ami_gen_rsa_keypair(int m, byte[] e,
	int len, byte[] seed) throws  AMI_KeyGenException;	        

    /**
     * Method to extract modulus and exponent of public key
     */
    public native void ami_extract_public_modexp(byte[] data)
	throws AMI_KeyGenException;

    /**
     * Method to extract modulus and exponent of private key
     */
    public native void ami_extract_private_modexp(byte[] data)
	throws AMI_KeyGenException;

    /**
     * Native method for DES 3Des ket generation algNAme is to be passed as 
     * DES or DES depending on what type  of key needs to be generated
     * 
     */
    public native void ami_gen_des3des_key(String algName) 
        throws  AMI_KeyGenException;	        

    /**
     * Native method for generating RC2 session  key.
     *
     * @param  strength Size of the RC2 key
     */
     public native void ami_gen_rc2_key(int strength)
	throws  AMI_KeyGenException;

    /**
     * Native method for generating RC4 session  key.
     *
     * @param  strength Size of the RC4 key
     */
    public native void ami_gen_rc4_key(int strength) 
	throws  AMI_KeyGenException;

    /**
     * Return the generated private key
     *
     * @return BigInteger The private key
     */
    public byte[] getPrivateKey()
    {
	return _privateKey;
    }

    /**
     * Return the generated public key
     *
     * @return BigInteger The public key
     */
    public byte[] getPublicKey()
    {
	return _publicKey;
    }

    /**
     * Return the generated secret key for symmetric keys
     *
     * @return BigInteger The secret key
     */
    public byte []  getSecretKey()
    {
	return _secretKey;
    }

    /**
     * return the iv as a big integer for des/3des
     */
    public byte[] getIV()
    {
	return _iv;
    }

    public int getIvLen()
    {
	return _ivLen;
    }

    public static BigInteger toBigInt(byte[] data) {
	String tempStr = new String("");
	for (int i = 0; i < data.length; i++) {
	    if ((data[i] & 0xFF) < 0x10)
		tempStr += "0";
		tempStr += Integer.toHexString(
		    data[i] & 0xFF).toUpperCase();
	}
	return (new BigInteger(tempStr, 16));
    }

    // For Asymmetric keys
    protected byte[] _publicKey;
    protected byte[] _publicModulus;
    protected byte[] _publicExponent;

    protected byte[] _privateKey;
    protected byte[] _privateModulus;
    protected byte[] _privateExponent;
    protected byte[] _pubExponent;
    protected byte[] _prime1;
    protected byte[] _prime2;
    protected byte[] _primeExponent1;
    protected byte[] _primeExponent2;
    protected byte[] _coefficient;

    // For Symmetric keys
    private byte[] _secretKey;
    private byte[] _iv;
    private int  _ivLen;
}
