/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_X509CertInfo.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.ca;

import java.io.IOException;
import java.io.OutputStream;
import java.io.InputStream;
import java.math.BigInteger;
import java.util.Date;
import java.security.PublicKey;
import java.security.cert.CertificateException;
import java.security.cert.CertificateParsingException;
import java.security.NoSuchAlgorithmException;

import sun.security.util.*;
import sun.security.x509.*;


/**
 * This class extends the X509CertInfo defined by the Javasoft sun packages.
 * 
 * It adds convenience methods to set the attributes for the certificate info.
 * It also adds a method to set the attributes using a certreq. 
 * 
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 * @see AMI_X509CertImpl
 * @see X509CertInfo
 */

public class AMI_X509CertInfo extends X509CertInfo {

   // Private data for validity and serial number calculations
    private Date _lastDate;
    private Date _firstDate;

    /**
     * Construct an uninitialized X509CertInfo on which <a href="#decode">
     * decode</a> must later be called (or which may be deserialized).
     */
    public AMI_X509CertInfo() { 
         _firstDate = new Date();
	 _lastDate  = new Date();
    }

    /**
     * Unmarshals a certificate from its encoded form, parsing the
     * encoded bytes.  This form of constructor is used by agents which
     * need to examine and use certificate contents.  That is, this is
     * one of the more commonly used constructors.  Note that the buffer
     * must include only a certificate, and no "garbage" may be left at
     * the end.  If you need to ignore data at the end of a certificate,
     * use another constructor.
     *
     * @param cert the encoded bytes, with no trailing data.
     * @exception CertificateParsingException on parsing errors.
     */
    public AMI_X509CertInfo(byte[] cert) throws CertificateParsingException {
         super(cert);
    }

    /**
     * Unmarshal a certificate from its encoded form, parsing a DER value.
     * This form of constructor is used by agents which need to examine
     * and use certificate contents.
     *
     * @param derVal the der value containing the encoded cert.
     * @exception CertificateParsingException on parsing errors.
     */
    public AMI_X509CertInfo(DerValue derVal)
	throws CertificateParsingException {
        super(derVal);
    }


    /* THESE ARE A SET OF CONVENIENCE ROUTINES .. to set the attributes */
    /* Alternatives to using the set() method of X509CertInfo class */

    /**
     * Set the version number of the certificate.
     *
     * @params val version
     * @exception CertificateException on invalid data.
     */
    public void setVersion(int val) throws CertificateException {
        if (val < 0 || val > 2) {
            throw new CertificateException("Invalid Version");
        }
	try {
	  version =  new CertificateVersion(val);
	} catch (IOException e) {
	   throw new CertificateException("IOException: Invalid Version, " + e);
	}
    }

    /**
     * Set the serial number of the certificate.
     *
     * @params val the Object class value for the CertificateSerialNumber
     */
    public void setSerialNumber(BigInteger serial) {

        serialNum = new CertificateSerialNumber(serial);
    }
    /**
     * Set the serial number of the certificate, after generating it.
     *
     * @params val the Object class value for the CertificateSerialNumber
     */
    public void setSerialNumber() {

        serialNum = new CertificateSerialNumber((int)
	    (_firstDate.getTime()/1000));
    }

    /**
     * Set the algorithm id of the certificate.
     *
     * @params algo The name of the algorithm
     * @exception CertificateException on invalid data.
     */
    public void setAlgorithm(String algo) throws CertificateException {
	try {

	AlgorithmId ai = null;
	if (algo.indexOf(".") != -1) {
        	ai = new AlgorithmId(new ObjectIdentifier(algo));
        	algId =  new CertificateAlgorithmId(ai);
	}
	else 
		algId =  new CertificateAlgorithmId(AlgorithmId.get(algo));
	} catch (Exception e) {
	  throw new CertificateException("Invalid Algorithm specified , " + e);
	}
    }

    /**
     * Set the issuer name of the certificate.
     *
     * @params val the Object class value for the issuer
     * @exception CertificateException on invalid data.
     */
    public void setIssuer(String name) throws CertificateException {
 
	try {
         issuer = new CertificateIssuerName(new X500Name(name));
	} catch (IOException e) {
	  throw new CertificateException("IOException: Invalid Name , " + e);
	}
    }

    /**
     * Set the issuer name of the certificate.
     *
     * @params val the Object class value for the issuer
     */
    public void setIssuer(X500Name name) {
 
         issuer = new CertificateIssuerName(name);
    }

    /**
     * Set the validity interval of the certificate.
     *
     * @params firstDate
     * @params lastDate
     */
    public void setValidity(Date firstDate, Date lastDate) {

        interval = new CertificateValidity(firstDate, lastDate);
    }
    /**
     * Set the validity interval of the certificate.
     *
     * @params time validity period
     */
    public void setValidity(long time) {

	_lastDate.setTime(_lastDate.getTime() + time * 1000);

        interval = new CertificateValidity(_firstDate, _lastDate);
    }

    /**
     * Set the subject name of the certificate.
     *
     * @params name Subject's DName
     * @exception CertificateException on invalid data.
     */
    public void setSubject(String name) throws CertificateException {
 
	try {
         subject = new CertificateSubjectName(new X500Name(name));
	} catch (IOException e) {
	  throw new CertificateException("IOException: Invalid Name , " + e);
	}
    }

    /**
     * Set the subject name of the certificate.
     *
     * @params name Subject's DName
     */
    public void setSubject(X500Name name) {
 
         subject = new CertificateSubjectName(name);
    }

    /**
     * Set the public key in the certificate.
     *
     * @params key value for the PublicKey
     */
    public void setKey(PublicKey key) {

        pubKey = new CertificateX509Key(key);
    }

    /**
     * Set the Issuer Unique Identity in the certificate.
     *
     * @params val byte[] containing the unique issuer id
     * @exception CertificateException
     */
    public void setIssuerUniqueId(byte[] val) throws CertificateException {
        if (version.compare(CertificateVersion.V2) < 0) {
            throw new CertificateException("Invalid version");
        }
        
        issuerUniqueId =  new CertificateIssuerUniqueIdentity(
	    new UniqueIdentity(val));
    }

    /**
     * Set the Subject Unique Identity in the certificate.
     *
     * @params val byte[] containing the unique subject id
     * @exception CertificateException
     */
    public void setSubjectUniqueId(byte[] val) throws CertificateException {
        if (version.compare(CertificateVersion.V2) < 0) {
            throw new CertificateException("Invalid version");
        }

        subjectUniqueId = new CertificateSubjectUniqueIdentity(
	    new UniqueIdentity(val));
    }

    /**
     * Set the extensions in the certificate.
     *
     * @params val the Object class value for the Extensions
     * @exception CertificateException
     */
    public void setExtensions(CertificateExtensions val)
	throws CertificateException {
        if (version.compare(CertificateVersion.V3) < 0) {
            throw new CertificateException("Invalid version");
        }
        extensions = val;
    }

    /**
     * Set the attributes from the certificate request object.
     *
     * @params certreq Certificate Request Object
     */
    public void setCertReqAttrs(AMI_CertReq certreq) {

        // Set the public Key
        setKey(certreq.getPublicKey());

	// Set the Subject DN
	setSubject(certreq.getSubject());
        
    }


}
