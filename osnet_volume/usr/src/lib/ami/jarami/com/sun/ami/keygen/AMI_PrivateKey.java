/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_PrivateKey.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.keygen;

import java.security.*;
import java.math.BigInteger;
import java.io.IOException;

import sun.security.util.DerValue;
import sun.security.util.BigInt;
import sun.security.util.DerOutputStream;
import sun.security.x509.AlgorithmId;
/**
 * This class implements the PrivateKey interface to create a AMI_PrivateKey, 
 * which acts as a psuedo private key, containing only the name and type of
 * the key, but not the actual key contents.
 *
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 * @see PrivateKey
 *
 */


public final class AMI_PrivateKey implements PrivateKey
{
	public AMI_PrivateKey()
	{
	}

	public AMI_PrivateKey(String alias, String algo)
	{
		_alias = alias;
		_algo = algo;
	}

	/**
	* Returns the name of the key generation algorithm
	*
	* @return String
	*/
	public String getAlgorithm()
	{
		return _algo;
	}

	/**
	* Returns the DER Encoded private key as a byte[]
	*
	* @return byte[]
	*/
	public byte[] getEncoded()
	{
		byte[] result = null;

		try {
			result = encode();
		} catch (InvalidKeyException e) {
		}

		return result;  
	}

	/**
	* Returns the format of encoding of the key(BER)
	*
	* @return String
	*/
	public String getFormat()
	{
		return null;
	}

	/**
	* Returns the name of the key generation algorithm
	*
	* @return String
	*/
	public String getAlias()
	{
	return _alias;
	}

	/**
	*
	*/
	public void setAlias(String alias)
	{
		_alias = alias;
	}

	/**
	*
	*/
	public void setAlgo(String algo)
	{
		_algo = algo;
	}

	public String toString()
	{
		String str = getAlgorithm() + ":\n Alias = " +
				_alias + " \n";
		str += "Key Type = " + _algo + "\n";

		return str;
	}


	private byte[] encode() throws InvalidKeyException {
	if (_encodedKey == null) {
		try {
			DerOutputStream out;

			out = new DerOutputStream();
			encode(out);
			_encodedKey = out.toByteArray();

		} catch (IOException e) {
			throw new InvalidKeyException(
				"IOException : " + e.getMessage());
		}
	}
		return copyEncodedKey(_encodedKey);
	}

	private void encode(DerOutputStream out) throws IOException
	{
		AlgorithmId algid = null;

		DerOutputStream tmp = new DerOutputStream();

		tmp.putInteger(new BigInt(_version.toByteArray()));

		if (_algo.equals("RSA"))
		 algid = new AlgorithmId(AlgorithmId.RSAEncryption_oid);
		else 
		if (_algo.equals("DSA"))
		 algid = new AlgorithmId(AlgorithmId.DSA_oid);
		else
		if (_algo.equals("Diffie-Hellman") || (_algo.equals("DH")))
		 algid = new AlgorithmId(AlgorithmId.DH_oid);

		algid.encode(tmp);
		tmp.putOctetString(_alias.getBytes());
		out.write(DerValue.tag_Sequence, tmp);
	}

	private byte[] copyEncodedKey(byte[] encodedKey) {
		int len = encodedKey.length;
		byte[] copy = new byte[len];
		System.arraycopy(encodedKey, 0, copy, 0, len);
		return copy;
	}

	private final BigInteger _version = BigInteger.valueOf(0);
	private byte[] _encodedKey;
	private String _alias;
	private String _algo;

}
