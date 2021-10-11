/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)SunAMI.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.common;

import java.security.AccessController;
import java.security.Provider;

/**
 * This class creates an AMI "provider", implementing 
 * <LI> Key generation algorithms  : RSA, RC2, RC4 .
 * <LI> Signature algorithms : MD2withRSA, MD5withRSA, SHAwithRSA
 * <LI> Crypto algorithms : RC4, RSA.
 * <LI> Key Factories:  RSA.
 * <LI> This provider provides the implementation which uses the AMI Server 
 * for cryptographic operations, in case the private key is not provided 
 * by the user.
 */

public final class SunAMI extends Provider {

    private static String _info = "Sun AMI Security Provider v1.0, " +
	    "RSA/RC2/RC4  key generation, RC2/RC4/RSA Crypto, " +
	    "RSA Signature, AMI KeyStore, X509 Certificates ";

    public SunAMI()
    {
	super("SunAMI", 1.0, _info);
       	AccessController.doPrivileged(new java.security.PrivilegedAction() {

	    public Object run() {

	    /*
	     * KeyPairGeneration engines
	     */
	    put("KeyPairGenerator.RSA",
		"com.sun.ami.keygen.AMI_RSAKeyPairGenerator");
	    put("KeyPairGenerator.DH",
		"com.sun.ami.dh.AMI_DHKeyPairGenerator");

	    /*
	     * KeyGeneration engines
	     */
	    put("KeyGenerator.RC2",
		"com.sun.ami.keygen.AMI_RC2KeyGenerator");
	    put("Alg.Alias.Signature.OID.1.2.840.113549.3.2", "RC2");
	    put("KeyGenerator.RC4",
		"com.sun.ami.keygen.AMI_RC4KeyGenerator");
	    put("Alg.Alias.Signature.OID.1.2.840.113549.3.4", "RC4");
	    put("Alg.Alias.Signature.OID.1.2.14.3.2.7", "DES");
	    put("Alg.Alias.Signature.OID.1.2.840.113549.3.7", "3DES");

	    /*
	     * Signature engines
	     */
	    put("Signature.MD5/RSA", 
		"com.sun.ami.sign.AMI_MD5RSASignature");
	    put("Signature.MD2/RSA", 
		"com.sun.ami.sign.AMI_MD2RSASignature");
	    put("Signature.SHA1/RSA", 
		"com.sun.ami.sign.AMI_SHA1RSASignature");
	    put("Alg.Alias.Signature.MD5withRSA", "MD5/RSA");
	    put("Alg.Alias.Signature.MD2withRSA", "MD2/RSA");
	    put("Alg.Alias.Signature.SHA1withRSA", "SHA1/RSA");
	    put("Alg.Alias.Signature.SHAwithRSA", "SHA1/RSA");
	    put("Alg.Alias.Signature.SHA/RSA", "SHA1/RSA");
	    put("Alg.Alias.Signature.SHA-1/RSA", "SHA1/RSA");

	    put("Signature.DSA", "com.sun.ami.sign.AMI_DSASignature");
	    put("Alg.Alias.Signature.SHA/DSA", "DSA");
	    put("Alg.Alias.Signature.SHA1/DSA", "DSA");
	    put("Alg.Alias.Signature.SHA1withDSA", "DSA");
	    put("Alg.Alias.Signature.SHA-1/DSA", "DSA");
	    put("Alg.Alias.Signature.DSS", "DSA");
	    put("Alg.Alias.Signature.OID.1.3.14.3.2.13", "DSA");
	    put("Alg.Alias.Signature.OID.1.3.14.3.2.27", "DSA");
	    put("Alg.Alias.Signature.OID.1.2.840.10040.4.3", "DSA");

	    /*
	     * Algorithm Parameter engines
	     */
	    put("AlgorithmParameters.RC2", 
		"com.sun.ami.keygen.AMI_RC2Parameters");
	    put("AlgorithmParameters.1.2.840.113549.3.2", 
		"com.sun.ami.keygen.AMI_RC2Parameters");

	    /*
	     * Cipher engines
	     */
	    put("Cipher.RC4", "com.sun.ami.crypto.AMI_RC4Cipher");
	    put("Cipher.RC2", "com.sun.ami.crypto.AMI_RC2Cipher");
	    put("Cipher.RSA", "com.sun.ami.crypto.AMI_RSACipher");

	    /*
	     * Digest engines
	     */
	    put("MessageDigest.MD2", "com.sun.ami.digest.AMI_MD2");
	    put("MessageDigest.MD5", "com.sun.ami.digest.AMI_MD5");
	    put("Alg.Alias.MessageDigest.SHA1", "SHA1");
	    put("Alg.Alias.MessageDigest.SHA-1", "SHA1");
	    put("MessageDigest.SHA1", "com.sun.ami.digest.AMI_SHA1");

	    /*
	     * KeyFactory engines
	     */
	    put("KeyFactory.RSA", "com.sun.ami.keygen.AMI_RSAKeyFactory");
	    put("KeyFactory.DH", "com.sun.ami.dh.AMI_DHKeyFactory");
	    put("KeyFactory.Diffie-Hellman",
		"com.sun.ami.dh.AMI_DHKeyFactory");

	    /*
	     * X509 certificate factory
	     */
	    put("CertificateFactory.X509", "com.sun.ami.ca.AMI_X509Factory");

	    /*
	     * KeyStore "amiks"
	     */
	    put("KeyStore.AMIKS", "com.sun.ami.keymgnt.AMI_KeyStore");

	    /*
	     * KeyStore "amicerts"
	     */
	    put("KeyStore.AMICERTS", "com.sun.ami.keymgnt.AMI_KeyStore_Certs");

	    return null;
	    }
	});
    }
}
