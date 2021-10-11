/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_KeyUtils.java	1.1 99/07/11 SMI"
 *
 */


package com.sun.ami.keygen;

import java.security.*;
import java.security.spec.X509EncodedKeySpec;
import java.security.spec.PKCS8EncodedKeySpec;
import java.security.spec.InvalidKeySpecException;
import java.io.IOException;
import java.math.BigInteger;

import sun.security.util.DerValue;
import sun.security.util.BigInt;
import sun.security.util.DerOutputStream;
import sun.security.x509.AlgorithmId;
 

/**
 * This class provides key utility functions . 
 *
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 */

public class AMI_KeyUtils {
  
    public static final BigInteger version = BigInteger.valueOf(0);

 
	/**
	* Extract the public key from a given DerValue input 
	*
	* @param DerValue input value
	* @return PublicKey
	* @exception IOException
	*/
    public static PublicKey parse(DerValue in) throws IOException
    {
        AlgorithmId     algorithm;
        PublicKey         subjectKey;

	
        if (in.tag != DerValue.tag_Sequence)
            throw new IOException("corrupt subject key");

        algorithm = AlgorithmId.parse(in.data.getDerValue());

        try {
            subjectKey = buildPublicKey(algorithm, in.data.getBitString());

        } catch (InvalidKeyException e) {
            throw new IOException("subject key, " + e.getMessage());
        }

        if (in.data.available() != 0)
            throw new IOException("excess subject key");
        return subjectKey;
    }


    static PublicKey buildPublicKey(AlgorithmId algid, byte[] key)
    throws IOException, InvalidKeyException
    {
        /*
         * Use the algid and key parameters to produce the ASN.1 encoding
         * of the key, which will then be used as the input to the
         * key factory.
         */
        DerOutputStream x509EncodedKeyStream = new DerOutputStream();
        encode(x509EncodedKeyStream, algid, key);
        X509EncodedKeySpec x509KeySpec
            = new X509EncodedKeySpec(x509EncodedKeyStream.toByteArray());

        try {
            // Instantiate the key factory of the appropriate algorithm
            KeyFactory keyFac = KeyFactory.getInstance(algid.getName());

            // Generate the public key
            PublicKey pubKey = keyFac.generatePublic(x509KeySpec);

            return pubKey;

        } catch (NoSuchAlgorithmException e) {
            throw new InvalidKeyException(e.toString());	    
        } catch (InvalidKeySpecException e) {
            throw new InvalidKeyException(e.toString());
        }
    }

    /*
     * Produce SubjectPublicKey encoding from algorithm id and key material.
     */
    static void encode(DerOutputStream out, AlgorithmId algid, byte[] key)
        throws IOException {
            DerOutputStream tmp = new DerOutputStream();
            algid.encode(tmp);
            tmp.putBitString(key);
            out.write(DerValue.tag_Sequence, tmp);
    }

	/**
	* Extract the private key from a given DerValue input 
	*
	* @param DerValue input value
	* @return PrivateKey
	* @exception IOException
	*/
    public static PrivateKey parseKey(DerValue in) throws IOException
    {
        AlgorithmId     algorithm;
        PrivateKey         subjectKey;

	
        if (in.tag != DerValue.tag_Sequence)
            throw new IOException("corrupt private key");

        BigInteger parsedVersion = in.data.getInteger().toBigInteger();

        if (!version.equals(parsedVersion)) {
            throw new IOException("version mismatch: (supported: " +
                                  version + ", parsed: " +
                                  parsedVersion);
        }

        algorithm = AlgorithmId.parse(in.data.getDerValue());

        try {
            subjectKey = buildPrivateKey(algorithm, in.data.getOctetString());

        } catch (InvalidKeyException e) {
            throw new IOException("private key, " + e.getMessage());
        }

        if (in.data.available() != 0)
            throw new IOException("excess private key");
        return subjectKey;
    }


    static PrivateKey buildPrivateKey(AlgorithmId algid, byte[] key)
    throws IOException, InvalidKeyException
    {
        /*
         * Use the algid and key parameters to produce the ASN.1 encoding
         * of the key, which will then be used as the input to the
         * key factory.
         */
        DerOutputStream pkcs8EncodedKeyStream = new DerOutputStream();
        encodeKey(pkcs8EncodedKeyStream, algid, key);
        PKCS8EncodedKeySpec pkcs8KeySpec
            = new PKCS8EncodedKeySpec(pkcs8EncodedKeyStream.toByteArray());

        try {
            // Instantiate the key factory of the appropriate algorithm
            KeyFactory keyFac = KeyFactory.getInstance(algid.getName());

            // Generate the private key
            PrivateKey privateKey = keyFac.generatePrivate(pkcs8KeySpec);

            return privateKey;

        } catch (NoSuchAlgorithmException e) {
            throw new InvalidKeyException(e.toString()); 	    
        } catch (InvalidKeySpecException e) {
            throw new InvalidKeyException(e.toString());
        }
    }

    /*
     * Produce PrivateKey encoding from algorithm id and key material.
     */
    static void encodeKey(DerOutputStream out, AlgorithmId algid, byte[] key)
        throws IOException {
            DerOutputStream tmp = new DerOutputStream();
            tmp.putInteger(new BigInt(version.toByteArray()));
            algid.encode(tmp);
            tmp.putOctetString(key);
            out.write(DerValue.tag_Sequence, tmp);
    }
}

