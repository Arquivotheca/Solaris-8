/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_CertReqInfo.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.ca;

import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;

import java.security.NoSuchAlgorithmException;
import java.security.InvalidKeyException;
import java.security.Signature;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.SignatureException;

import sun.security.util.*;	// DER
import sun.security.x509.X500Name;
import sun.security.x509.X500Signer;
import sun.security.pkcs.PKCS10Attributes;
import sun.security.x509.AlgorithmId;

import com.sun.ami.keygen.AMI_KeyUtils;

/**
 *
 * The AMI_CertReqInfo class represents PKCS10 certreq information.
 *
 * <P>PKCS10 certreqs have the following data elements, including:<UL>
 *
 * <LI>The <em>Subject Name</em>, an X.500 Distinguished Name for
 *      the entity (subject) for which the certreq was issued.
 *
 * <LI>The <em>Subject Public Key</em>, the public key of the subject.
 *
 * <LI>The <em>Version</em>, Cert Req version
 *
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 */

public class AMI_CertReqInfo extends Object {

    /**
     * PKCS10CertReq  Version 1     */
    public static final int     V1 = 0;

    /**
     * Constructs an uninitialised AMI_CertReqInfo.
     * 
     */

    public AMI_CertReqInfo() {
    }

    /**
    * Unmarshals cert request info  from its encoded form, parsing the
    * encoded bytes. 
    *
    * @param certReqData the encoded bytes, with no trailing padding.
    * @exception AMI_CertReqException on parsing and initialization errors.
    */
    public AMI_CertReqInfo(byte[] certReqData) throws AMI_CertReqException
    {

    try {
            DerValue	in = new DerValue(certReqData);

            parse(in);
        } catch (IOException e) {
             throw new AMI_CertReqException("Unable to initialize, " + e);
        }	
    }

    /**
    * Unmarshals cert request info  from the DerValue
    *
    * @param val the DerValue
    * @exception AMI_CertReqException on parsing and initialization errors.
    */
    public AMI_CertReqInfo(DerValue val) throws AMI_CertReqException
    {

    try {
            parse(val);
        } catch (IOException e) {
             throw new AMI_CertReqException("Unable to initialize, " + e);
        }	
    }

    /**
     * Decode a PKCS10 certificate from an input stream.
     *
     * @param in an input stream 
     * @exception AMI_CertReqException on decoding errors.
     * @exception IOException on other errors.
     */
    public void decode(InputStream in)
    throws AMI_CertReqException, IOException {
        DerValue	val = new DerValue(in);

        parse(val);
    }

    /**
     * Appends the cert req info to an output stream.
     *
     * @param out an output stream to which the cert req info is appended.
     * @exception AMI_CertReqException on encoding errors.
     * @exception IOException on other errors.
     */
    public void encode(OutputStream out)
    throws AMI_CertReqException, IOException {
        if (_rawCertReqInfo == null) {
            DerOutputStream tmp = new DerOutputStream();
            emit(tmp);
            _rawCertReqInfo = tmp.toByteArray();
        }
        out.write(_rawCertReqInfo);
    }

    /**
    *  Set the Subject's dname 
    */
    public void setSubject(String subject) throws IOException {

          _subject = new X500Name(subject);
    }

    /**
    *  Set the Subject's dname 
    */
    public void setSubject(X500Name subject) {

          _subject = subject;
    }

    /**
    *  Set the Public key of the requestor
    */
    public void setPublicKey(PublicKey publicKey) {
          _publicKey =  publicKey;
    }

     
    /**
    *  Set the Attribute Set 
    */
    public void setAttributeSet(PKCS10Attributes attributeSet) {
          _attributeSet =  attributeSet;
    }

    /**
    *  Set the Version
    */
    public void setVersion(int version) {
          _version =  version;
    }


    /**
    * Returns the subject's name.
    */
    public X500Name getSubject()
	{ return _subject; }


    /**
    * Returns the subject's public key.
    */
    public PublicKey getPublicKey()
	{ return _publicKey; }


    /**
    * Returns the additional attributes requested.
    */
    public PKCS10Attributes getAttributes()
	{ return _attributeSet; }


    /**
    * Returns the Version.
    */
    public int getVersion()
	{ return _version; }
     
    /*
     * This routine unmarshals the certreq information.
     */
    private void parse(DerValue val)
    throws AMI_CertReqException, IOException {
        DerInputStream	in;
        DerValue	tmp;

        if (val.tag != DerValue.tag_Sequence) {
            throw new AMI_CertReqException("signed fields invalid");
        }
        _rawCertReqInfo = val.toByteArray();

        in = val.data;

        // version
        tmp = in.getDerValue();

        _version =  tmp.getInteger().toInt();

        // subject name
        _subject = new X500Name(in);

	// public key
	tmp = in.getDerValue();

	_publicKey = AMI_KeyUtils.parse(tmp);

	// attribute set
	_attributeSet = new PKCS10Attributes(in);

        // If more data available, throw error.	
	if (in.available() != 0) {
	   throw new AMI_CertReqException("excess cert data = " +
		in.available());
	}
    }

    /*
    * Marshal the contents of "raw" certreq info into a DER sequence.
    */
    private void emit(DerOutputStream out)
    throws AMI_CertReqException, IOException {
        DerOutputStream tmp = new DerOutputStream();

        // version number
        tmp.putInteger(new BigInt(_version));

        // Encode subject (principal) and associated key
        _subject.encode(tmp);       
        tmp.write(_publicKey.getEncoded());

        _attributeSet.encode(tmp);

        // Wrap the data; encoding of the "raw" certreq is now complete.
        out.write(DerValue.tag_Sequence, tmp);
    }

    /**
     * Provides a short description of this information.
     */
    public String toString() {
	return ("[Information:\n"
	    + " version: <" + _version + ">" + "\n"
	    + _publicKey.toString()
	    + " \nsubject: <" + _subject.toString() + ">" + "\n"
	    + "\n]");
    }

    // Certificate Request Info Data 
    private X500Name		_subject;
    private PublicKey		_publicKey;
    private PKCS10Attributes	_attributeSet = new PKCS10Attributes();
    private int                 _version = V1;

   // DER encoded CertReqInfo data
    private byte[]	_rawCertReqInfo = null;
}

