/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_DHPrivateKey.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.dh;

import java.io.*;
import java.math.BigInteger;
import java.security.InvalidKeyException;
import java.security.PrivateKey;

import com.sun.ami.utils.*;
import sun.security.util.BigInt;

/**
 * A private key in PKCS#8 format for the Diffie-Hellman key agreement
 * algorithm.
 *
 */

public final class AMI_DHPrivateKey implements PrivateKey, Serializable {

    // the private key
    private BigInteger x;

    // the key bytes, without the algorithm information
    private byte[] key;

    // the encoded key
    private byte[] encodedKey;

    // the prime modulus
    private BigInteger p;

    // the base generator
    private BigInteger g;

    // the private-value length
    private int l;

    // Version
    private BigInteger version = BigInteger.valueOf(0);

    private int DH_data[] = { 1, 2, 840, 113549, 1, 3, 1 };

    /**
     * Make a DH private key out of a private value <code>x</code>, a prime
     * modulus <code>p</code>, and a base generator <code>g</code>.
     *
     * @param x the private value
     * @param p the prime modulus
     * @param g the base generator
     *
     * @exception InvalidKeyException if the key cannot be encoded
     */
    public AMI_DHPrivateKey(BigInteger x, BigInteger p, BigInteger g) 
	throws InvalidKeyException {
	    this.x = x;
	    this.p = p;
	    this.g = g;
	    try {
		this.key = new DerValue(DerValue.tag_Integer, 
					this.x.toByteArray()).toByteArray();
		this.encodedKey = getEncoded();
	    } catch (IOException e) {
		throw new InvalidKeyException("Cannot produce ASN.1 encoding");
	    }
    }

    /**
     * Make a DH private key out of a private value <code>x</code>, a prime
     * modulus <code>p</code>, a base generator <code>g</code>, and a
     * private-value length <code>l</code>.
     *
     * @param x the private value
     * @param p the prime modulus
     * @param g the base generator
     * @param l the private-value length
     *
     * @exception InvalidKeyException if the key cannot be encoded
     */
    public AMI_DHPrivateKey(BigInteger x, BigInteger p, BigInteger g, int l) 
	throws InvalidKeyException {
	    this.x = x;
	    this.p = p;
	    this.g = g;
	    this.l = l;
	    try {
		this.key = new DerValue(DerValue.tag_Integer, 
					this.x.toByteArray()).toByteArray();
		this.encodedKey = getEncoded();
	    } catch (IOException e) {
		throw new InvalidKeyException("Cannot produce ASN.1 encoding");
	    }
    }

    /**
     * Make a DH private key from its DER encoding (PKCS #8).
     *
     * @param encodedKey the encoded key
     *
     * @exception InvalidKeyException if the encoded key does not represent
     * a Diffie-Hellman private key
     */
    public AMI_DHPrivateKey(byte[] encodedKey) throws InvalidKeyException {
	InputStream inStream = new ByteArrayInputStream(encodedKey);
	try {
	    DerValue derKeyVal = new DerValue(inStream);
	    if (derKeyVal.tag != DerValue.tag_Sequence) {
		throw new InvalidKeyException("Invalid key format");
	    }

	    /*
	     * Parse the algorithm identifier
	     */
	    DerValue algid = derKeyVal.data.getDerValue();
	    if (algid.tag != DerValue.tag_Sequence) {
		throw new InvalidKeyException("AlgId is not a SEQUENCE");
	    }
	    DerInputStream derInStream = algid.toDerInputStream();
	    ObjectIdentifier oid = derInStream.getOID();
	    if (derInStream.available() == 0) {
		throw new InvalidKeyException("Parameters missing");
	    }

	    /*
	     * Parse the parameters
	     */
	    DerValue params = derInStream.getDerValue();
	    if (params.tag == DerValue.tag_Null) {
		throw new InvalidKeyException("Null parameters");
	    }
	    if (params.tag != DerValue.tag_Sequence) {
		throw new InvalidKeyException("Parameters not a SEQUENCE");
	    }
	    params.data.reset();
	    this.p = params.data.getInteger().toBigInteger();
	    this.g = params.data.getInteger().toBigInteger();
	    // Private-value length is OPTIONAL
	    if (params.data.available() != 0) {
		this.l = params.data.getInteger().toInt();
	    } else {
		this.l = 0;
	    }
	    if (params.data.available() != 0) {
		throw new InvalidKeyException("Extra parameter data");
	    }

	    /*
	     * Parse the key
	     */
	    this.key = derKeyVal.data.getBitString();
	    parseKeyBits();
	    if (derKeyVal.data.available() != 0) {
		throw new InvalidKeyException("Excess key data");
	    }

	    this.encodedKey = copyEncodedKey(encodedKey);

	} catch (NumberFormatException e) {
	    throw new InvalidKeyException("Private-value length too big");

	} catch (IOException e) {
       	    throw new InvalidKeyException(e.toString());
	}
    }

    /**
     * Returns the encoding format of this key: "PKCS#8"
     */
    public String getFormat() {
	return "PKCS#8";
    }

    /**
     * Returns the name of the algorithm associated with this key: "DH"
     */
    public String getAlgorithm() {
	return "DH";
    }

    /**
     * Get the encoding of the key.
     */
    public synchronized byte[] getEncoded() {
	if (this.encodedKey == null) {
	    try {
		DerOutputStream algid = new DerOutputStream();

		// store oid in algid
		algid.putOID(new ObjectIdentifier(DH_data));
		

		// encode parameters
		DerOutputStream params = new DerOutputStream();
		params.putInteger(new BigInt(this.p.toByteArray()));
		params.putInteger(new BigInt(this.g.toByteArray()));
		if (this.l > 0) { // Private-value length is OPTIONAL
		    params.putInteger(new BigInt(BigInteger.valueOf(this.l)));
		}
		// wrap parameters into SEQUENCE
		DerValue paramSequence = new DerValue(DerValue.tag_Sequence,
						      params.toByteArray());
		// store parameter SEQUENCE in algid
		algid.putDerValue(paramSequence);

		// wrap algid into SEQUENCE, and store it in key encoding
		DerOutputStream tmpDerKey = new DerOutputStream();
		System.out.println("Adding new version integer");
		tmpDerKey.putInteger(new BigInt(version.toByteArray()));
		tmpDerKey.write(DerValue.tag_Sequence, algid);

		// store key data
		tmpDerKey.putOctetString(key);

		// wrap algid and key into SEQUENCE
		DerOutputStream derKey = new DerOutputStream();
		derKey.write(DerValue.tag_Sequence, tmpDerKey);
		this.encodedKey = derKey.toByteArray();
	    } catch (IOException e) {
		return null;
	    }
	}
	return copyEncodedKey(this.encodedKey);
    }

    /**
     * Returns the private value, <code>x</code>.
     *
     * @return the private value, <code>x</code>
     */
    public BigInteger getX() {
	return this.x;
    }

    /**
     * Returns the key parameters.
     *
     * @return the key parameters
     */
    public AMI_DHParameterSpec getParams() {
	return new AMI_DHParameterSpec(this.p, this.g, this.l);
    }

    public String toString() {
	return "Sun DH Private Key \nparameters:" + "\nx: " + // XXX + algid
	    (new BigInt(x)).toString() + "\n";
    }

    private void parseKeyBits() throws InvalidKeyException {
	DerInputStream in = new DerInputStream(this.key);
	try {
	    this.x = in.getInteger().toBigInteger();
	} catch (IOException e) {
	    throw new InvalidKeyException(e.toString());
	}
    }

    /*
     * Make a copy of the encoded key.
     */
    private byte[] copyEncodedKey(byte[] encodedKey) {
	byte[] copy = new byte[encodedKey.length];
	System.arraycopy(encodedKey, 0, copy, 0, encodedKey.length);
	return copy;
    }
}
