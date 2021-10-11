/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_asn1_common.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.utils;

import java.io.*;
import java.math.BigInteger;
import java.util.Date;
import java.lang.StringBuffer;
import java.security.PublicKey;
import java.security.SecureRandom;
import java.security.cert.*;
import java.security.PrivateKey;
import java.security.Signature;
import java.security.Security;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.NoSuchProviderException;
import java.security.NoSuchAlgorithmException;
import java.security.UnrecoverableKeyException;
import java.security.MessageDigest;

import sun.security.x509.X509CertImpl;
import sun.security.x509.X509Key;
import sun.security.util.BigInt;

import com.sun.ami.utils.DerOutputStream;
import com.sun.ami.utils.DerInputStream;
import com.sun.ami.utils.DerValue;
import com.sun.ami.common.AMI_DigestInfo;
import com.sun.ami.keymgnt.AMI_KeyMgntClient;
import com.sun.ami.keymgnt.AMI_KeyStore_Certs;
import com.sun.ami.keymgnt.AMI_KeyMgntException;
import com.sun.ami.common.*;
import com.sun.ami.AMI_Exception;
import com.sun.ami.ca.AMI_X509CertInfo;
import com.sun.ami.crypto.AMI_Crypto;

/**
 * The following class acts as an intermediary between AMI and 
 * those ANS1 C APIs which require AMI services.
 *
 * It is necessary that C APIs use JNI in order to interface
 * directly with java classes (AMI). But since calling 
 * Java from C is cumbersome and tedious, reducing the 
 * the number of Java methods invoked through JNI is worthwhile.
 *
 * This class accomplishes our aim by providing access
 * AMI services in a manner tailored specifically to the 
 * C APIs, while at the same time minimizing the number
 * of JNI calls required.
 */

class AMI_asn1_common {

    public static byte[] encodeRC2Params(byte[] iv, int effectiveKeySize)
	throws IOException, AMI_KeyMgntException {

	    AMI_Debug.debugln(3, "In Java :: encode RC2 params ");

	    DerOutputStream tmp = new DerOutputStream();
            DerOutputStream dos = new DerOutputStream();
            tmp.putOctetString(iv);
	    tmp.putInteger(new BigInt(effectiveKeySize));

            dos.write(DerValue.tag_Sequence, tmp);
            return dos.toByteArray();
        }
				
        public static byte[] decodeRC2Params1(byte[] encoded)
                throws IOException, AMI_KeyMgntException
        {
                AMI_Debug.debugln(3, "In Java :: decode RC2 params ");

		DerInputStream dis = new DerInputStream(encoded);
		
		DerValue[] values = dis.getSequence(2);
		byte[] iv = values[0].getOctetString();
		int size = values[1].getInteger().toInt();

		return iv;
        }

        public static int decodeRC2Params2(byte[] encoded)
               throws IOException, AMI_KeyMgntException
        {
                AMI_Debug.debugln(3, "In Java :: decode RC2 params ");

		DerInputStream dis = new DerInputStream(encoded);
		
		DerValue[] values = dis.getSequence(2);
		byte[] iv = values[0].getOctetString();
		int size = values[1].getInteger().toInt();

		return size;
        }


	public static byte[] decodeOctetString(byte[] data) 
		throws IOException, AMI_KeyMgntException
	{
	        AMI_Debug.debugln(3, "In Java :: decode Octet string");

		byte[] retData = null;
		DerInputStream dis = new DerInputStream(data);

		retData = dis.getOctetString();
		return retData;
	} 

	public static byte[] encodeOctetString(byte[] data)
	throws IOException, AMI_KeyMgntException
	{
                AMI_Debug.debugln(3, "In Java :: encode Octet string");

		DerOutputStream dos = new DerOutputStream();
		dos.putOctetString(data);

		return dos.toByteArray();
	} 
  
	public static String decodePrintableString(byte[] data)
		throws IOException, AMI_KeyMgntException
	{
                AMI_Debug.debugln(3, "In Java :: decode Printable string");

		String retData = null;
		DerInputStream dis = new DerInputStream(data);

		retData = dis.getPrintableString();

		return retData;
	} 
	

	public static byte[] encodePrintableString(String data) 
	throws IOException, AMI_KeyMgntException
	{
                AMI_Debug.debugln(3, "In Java :: encode Printable string");

		DerOutputStream dos = new DerOutputStream();
		dos.putPrintableString(data);

		byte[] barray = dos.toByteArray();
		return dos.toByteArray();
	} 
	 

	public static String decodeIA5String(byte[] data)
	throws IOException, AMI_KeyMgntException
	{
                AMI_Debug.debugln(3, "In Java :: decode IA5 string");

		String retData = null;
		DerInputStream dis = new DerInputStream(data);

		retData = dis.getPrintableString();
		return retData;
	} 
	

	public static byte[] encodeIA5String(String data) 
	throws IOException, AMI_KeyMgntException
	{
                AMI_Debug.debugln(3, "In Java :: encode IA5 string");

		DerOutputStream dos = new DerOutputStream();
		dos.putPrintableString(data);

		return dos.toByteArray();
	} 
	 

	public static byte[] ami_random(int length) 
	throws IOException, AMI_KeyMgntException,
	NoSuchAlgorithmException
	{
                AMI_Debug.debugln(3, "In Java :: ami_random");

		SecureRandom secureRand = SecureRandom.getInstance("SHA1PRNG");
		byte[] num = new byte[length];
		secureRand.nextBytes(num);

		return num;
	}

	public static AMI_C_Certs[] getCertificates(String DN)
        throws Exception
	{
	        AMI_C_Certs[] c_certs = null;
	        Certificate cert[]  = null;

                AMI_Debug.debugln(3, "In Java :: getCertificates for " + DN); 
  	        AMI_KeyStore_Certs keystore = new AMI_KeyStore_Certs();

                AMI_Debug.debugln(3, "lading keystore "); 

                AMI_Debug.debugln(3, "getting cert chain "); 

	        try {
		    cert = keystore.engineGetCertificates(DN);
	        } catch (Exception e) {
		    // e.printStackTrace();
		    return null;
	        }		


		if (cert == null || cert.length == 0)
		    return null;

		AMI_Debug.debugln(3, "Found # of Certs = " + cert.length);

		AMI_Debug.debugln(3, "converting cert chain "); 

	        try {
		    c_certs = AMI_C_Certs.convertCerts(cert);
	        } catch (Exception e) {
		    // e.printStackTrace();
		    return null;
	        }
	      	      
                AMI_Debug.debugln(3, "Returning Certs = " + c_certs.length);
	        return c_certs;
	}

	public static byte[] encodeCertificateInfo(int version,
	    String issName, String subName, String algo, long notBefore,
	    long notAfter, byte[] serial, byte[] pubKey)
	    throws Exception {
		AMI_Debug.debugln(3, "In encodeCertificateInfo (Java)");
		String digestAlgo = null;

		AMI_X509CertInfo _certInfo = new AMI_X509CertInfo();

		_certInfo.setVersion(version);
		_certInfo.setIssuer(issName);
		_certInfo.setSubject(subName);
		_certInfo.setAlgorithm(algo);
		_certInfo.setValidity(new Date(notBefore * 1000),
		    new Date(notAfter * 1000));
		_certInfo.setSerialNumber(new BigInteger(new String(serial)));
		_certInfo.setKey(X509Key.parse(
		    new sun.security.util.DerValue(pubKey)));

		byte[] encoded = _certInfo.getEncodedInfo();

		if (algo.equals("1.2.840.113549.1.1.2"))
			digestAlgo = "MD2";
		else if (algo.equals("1.2.840.113549.1.1.4"))
			digestAlgo = "MD5";
		else if (algo.equals("1.2.840.113549.1.1.5"))
			digestAlgo = "SHA1";
		else if (algo.equals("1.2.840.10040.4.3")) {
			// It is SHA1/DSA PKIX data
			return (encoded);
		}

		MessageDigest md = MessageDigest.getInstance(digestAlgo);
		byte[] digestedData = md.digest(encoded);
		AMI_DigestInfo digInfo = new AMI_DigestInfo(digestAlgo,
		    digestedData);
		return digInfo.encode();
	}
}
                                         
