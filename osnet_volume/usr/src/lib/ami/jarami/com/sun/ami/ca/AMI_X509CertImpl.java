/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_X509CertImpl.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.ca;

import java.io.IOException;
import java.io.Serializable;
import java.io.InputStream;
import java.io.ObjectInputStream;
import java.io.OutputStream;
import java.io.PrintStream;
import java.io.ObjectOutputStream;
import java.math.BigInteger;
import java.security.cert.*;
import java.security.*;
import java.util.Collections;
import java.util.Date;
import java.util.Enumeration;
import java.util.Set;
import java.util.HashSet;

import sun.security.util.*;
import sun.misc.BASE64Encoder;

import sun.security.x509.*;

/**
 *
 * The AMI_X509CertImpl class represents an X.509 certificate.
 * These certificates
 * are widely used to support authentication and other functionality in
 * Internet security systems.
 *
 * <P>These certificates are managed and vouched for by <em>Certificate
 * Authorities</em> (CAs).  CAs are services which create certificates by
 * placing data in the X.509 standard format and then digitally signing
 *
 * This class extends the X509CertImpl defined by JavaSoft in the Sun packages, 
 * to :
 * -- add a method to print the certificate in a printable Base64
 * encoded format.
 * -- sign the certificate with private key ( without specifying the algo, the
 * algorithm from the X509CertInfo object contained in the certificate is used)
 *
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 * @see X509CertImpl
 * @see X509Certificate
 */


public class AMI_X509CertImpl extends X509CertImpl
{

    public AMI_X509CertImpl() {
    }


    public AMI_X509CertImpl(byte[] certData) throws CertificateException {
         super(certData);
    }

    /**
     * unmarshals an X.509 certificate from an input stream.
     *
     * @param in an input stream holding at least one certificate
     * @exception CertificateException on parsing and initialization errors.
     */
    public AMI_X509CertImpl(InputStream in) throws CertificateException {
         super(in);
    }


    /**
     * Construct an initialized X509 Certificate. The certificate is stored
     * in raw form and has to be signed to be useful.
     *
     * @params info the X509CertificateInfo which the Certificate is to be
     *              created from.
     */
    public AMI_X509CertImpl(X509CertInfo certInfo) {
         super(certInfo);
    }

    public AMI_X509CertImpl(AMI_X509CertInfo certInfo) {
         super(certInfo);
    }

    /**
     * Unmarshal a certificate from its encoded form, parsing a DER value.
     * This form of constructor is used by agents which need to examine
     * and use certificate contents.
     *
     * @param derVal the der value containing the encoded cert.
     * @exception CertificateException on parsing and initialization errors.
     */
    public AMI_X509CertImpl(DerValue derVal) throws CertificateException {
         super(derVal);
    }



    /**
     * Creates an X.509 certificate, and signs it using the key
     * passed 
     * This operation is used to implement the certificate generation
     * functionality of a certificate authority.
     *
     * @param key the private key used for signing.
     *
     * @exception InvalidKeyException on incorrect key.
     * @exception NoSuchAlgorithmException on unsupported signature
     * algorithms.
     * @exception NoSuchProviderException if there's no default provider.
     * @exception SignatureException on signature errors.
     * @exception CertificateException on encoding errors.
     */
    public void sign(PrivateKey key)
    throws CertificateException, NoSuchAlgorithmException,
        InvalidKeyException, NoSuchProviderException,
	SignatureException, IOException {
	  
	String algorithm = ((AlgorithmId)((CertificateAlgorithmId) info.get(
	    X509CertInfo.ALGORITHM_ID)).get(
	    CertificateAlgorithmId.ALGORITHM)).getName();

	// System.out.println("AMI_X509CertImpl::Sign Algo : " + algorithm);
        sign(key, algorithm, null);
    }

    /* 
    * Writes out the certificate in printable Base 64 encoded format to the 
    * output stream specified.
    * @param PrintStream out The output stream.
    * @throws IOException If unable to write to the output stream
    * @throws CertificateException If certificate is not created yet
    */
    public void print(PrintStream out)
    throws IOException, CertificateException
    {
	if (getEncoded() == null)
	    throw new CertificateException("Certificate not created yet");
	
	BASE64Encoder encoder = new BASE64Encoder();

	out.println("-----BEGIN CERTIFICATE-----");
	encoder.encodeBuffer(getEncoded(), out);
	out.println("-----END CERTIFICATE-----");
    }

    
}
