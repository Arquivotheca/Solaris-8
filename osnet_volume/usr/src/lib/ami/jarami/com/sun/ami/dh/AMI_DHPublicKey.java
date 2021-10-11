/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_DHPublicKey.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.dh;

import java.io.*;
import java.math.BigInteger;
import java.security.InvalidKeyException;
import java.security.InvalidAlgorithmParameterException;
import java.security.PublicKey;

import com.sun.ami.utils.*;
import sun.security.util.BigInt;

/**
 * A public key in X.509 format for the Diffie-Hellman key agreement algorithm.
 *
 */

public final class AMI_DHPublicKey implements PublicKey, Serializable {

	// the public key
	private BigInteger y;

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

	private int DH_data[] = { 1, 2, 840, 113549, 1, 3, 1 };

	/**
	 * Make a DH public key out of a public value <code>y</code>, a prime
	 * modulus <code>p</code>, and a base generator <code>g</code>.
	 *
	 * @param y the public value
	 * @param p the prime modulus
	 * @param g the base generator
	 *
	 * @exception InvalidKeyException if the key cannot be encoded
	 */
	public AMI_DHPublicKey(BigInteger y, BigInteger p, BigInteger g) 
	    throws InvalidKeyException {
		this.y = y;
		this.p = p;
		this.g = g;
		try {
			this.key = new DerValue(DerValue.tag_Integer, 
			    this.y.toByteArray()).toByteArray();
			this.encodedKey = getEncoded();
		} catch (IOException e) {
			throw new InvalidKeyException(
			    "Cannot produce ASN.1 encoding");
		}
	}

	/**
	 * Make a DH public key out of a public value <code>y</code>, a prime
	 * modulus <code>p</code>, a base generator <code>g</code>, and a
	 * private-value length <code>l</code>.
	 *
	 * @param y the public value
	 * @param p the prime modulus
	 * @param g the base generator
	 * @param l the private-value length
	 *
	 * @exception InvalidKeyException if the key cannot be encoded
	 */
	public AMI_DHPublicKey(BigInteger y, BigInteger p, BigInteger g, int l) 
	throws InvalidKeyException {
		this.y = y;
		this.p = p;
		this.g = g;
		this.l = l;
		try {
			this.key = new DerValue(DerValue.tag_Integer, 
			    this.y.toByteArray()).toByteArray();
			this.encodedKey = getEncoded();
		} catch (IOException e) {
			throw new InvalidKeyException(
			    "Cannot produce ASN.1 encoding");
		}
	}
	
	/**
	 * Make a DH public key from its DER encoding (X.509).
	 *
	 * @param encodedKey the encoded key
	 *
	 * @exception InvalidKeyException if the encoded key does not represent
	 * a Diffie-Hellman public key
	 */
	public AMI_DHPublicKey(byte[] encodedKey) throws InvalidKeyException {
		InputStream inStream = new ByteArrayInputStream(encodedKey);
		try {
			DerValue derKeyVal = new DerValue(inStream);
			if (derKeyVal.tag != DerValue.tag_Sequence) {
				throw new InvalidKeyException(
				    "Invalid key format");
			}

			/*
			 * Parse the algorithm identifier
			 */
			DerValue algid = derKeyVal.data.getDerValue();
			if (algid.tag != DerValue.tag_Sequence) {
				throw new InvalidKeyException(
				    "AlgId is not a SEQUENCE");
			}
			DerInputStream derInStream = algid.toDerInputStream();
			ObjectIdentifier oid = derInStream.getOID();
			if (derInStream.available() == 0) {
				throw new InvalidKeyException(
				    "Parameters missing");
			}

			/*
			 * Parse the parameters
			 */
			DerValue params = derInStream.getDerValue();
			if (params.tag == DerValue.tag_Null) {
				throw new InvalidKeyException(
				    "Null parameters");
			}
			if (params.tag != DerValue.tag_Sequence) {
				throw new InvalidKeyException(
				    "Parameters not a SEQUENCE");
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
				throw new InvalidKeyException(
				    "Extra parameter data");
			}

			/*
			 * Parse the key
			 */
			this.key = derKeyVal.data.getBitString();
			parseKeyBits();
			if (derKeyVal.data.available() != 0) {
				throw new InvalidKeyException(
				    "Excess key data");
			}
			this.encodedKey = copyEncodedKey(encodedKey);
		} catch (NumberFormatException e) {
			throw new InvalidKeyException(
			    "Private-value length too big");
		} catch (IOException e) {
	   		throw new InvalidKeyException(e.toString());
		}
	}

	/**
	 * Returns the encoding format of this key: "X.509"
	 */
	public String getFormat() {
		return "X.509";
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
				params.putInteger(new BigInt(
				    this.p.toByteArray()));
				params.putInteger(new BigInt(
				    this.g.toByteArray()));
				if (this.l > 0) {
					// Private-value length is OPTIONAL
					params.putInteger(new BigInt(
					    BigInteger.valueOf(this.l)));
				}
				// wrap parameters into SEQUENCE
				DerValue paramSequence = new DerValue(
				    DerValue.tag_Sequence,
				    params.toByteArray());
				// store parameter SEQUENCE in algid
				algid.putDerValue(paramSequence);

				// wrap algid into SEQUENCE, and store it
				// in key encoding
				DerOutputStream tmpDerKey =
				    new DerOutputStream();
				tmpDerKey.write(DerValue.tag_Sequence, algid);

				// store key data
				tmpDerKey.putBitString(this.key);

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
	 * Returns the public value, <code>y</code>.
	 *
	 * @return the public value, <code>y</code>
	 */
	public BigInteger getY() {
		return this.y;
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
		return "Sun DH Public Key\n"
		    + "y: " + (new BigInt(y)).toString() + "\n"
		    + "parameters:\n"
		    + "	p:\n" + (new BigInt(this.p)).toString() + "\n"
		    + "	g:\n" + (new BigInt(this.g)).toString();
	}

	private void parseKeyBits() throws InvalidKeyException {
		try {
			DerInputStream in = new DerInputStream(this.key);
			this.y = in.getInteger().toBigInteger();
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
