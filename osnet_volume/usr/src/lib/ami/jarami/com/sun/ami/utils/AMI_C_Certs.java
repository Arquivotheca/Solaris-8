/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_C_Certs.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.utils;

import java.lang.*;
import java.util.*;
import java.io.*;
import java.security.*;
import java.security.cert.*;
import java.security.cert.Certificate;

import com.sun.ami.AMI_Exception;
import com.sun.ami.common.*;
import com.sun.ami.ca.AMI_X509CertInfo;
import com.sun.ami.ca.AMI_X509CertImpl;

/**
 * This class contains native methods to convert a Java Certificate into 
 * a C Certificate structure.
 * This class is used by the C API's.
 */

public class AMI_C_Certs {

    public AMI_C_Certs() {
    }

    public static AMI_C_Certs[] convertCerts(Certificate[] cert) 
        throws Exception {
	    AMI_C_Certs [] c_cert  = new AMI_C_Certs[cert.length];

	    for (int ii = 0; ii < cert.length; ii++) {
	        System.out.println("Converting ii = " + ii);

	        c_cert[ii].setCertificate(cert[ii]);
	    }
	    return c_cert;
    }
    
    public void setCertificate(Certificate cert) {
	    _certificate = (X509Certificate)cert;
    }

	public void setCertificate(byte[] cert_encoded) {
		ByteArrayInputStream bais =
		    new ByteArrayInputStream(cert_encoded);
		CertificateFactory cf = null;
		try {
			cf = CertificateFactory.getInstance("X509",
			    "SunAMI");
			_certificate = (X509Certificate)
			    cf.generateCertificate(bais);
		} catch (NoSuchProviderException e) {
			_certificate = null;
			return;
		} catch (CertificateException f) {
			_certificate = null;
			return;
		}
	}

    public byte[] getSignature() {
	    return _certificate.getSignature();
    }

    public String getKeyAlgName() {
	    return _certificate.getPublicKey().getAlgorithm();
    }

    public String getSigAlgName() {
	    return _certificate.getSigAlgName();
    }

    public byte[] getPublicKey() {
	    return _certificate.getPublicKey().getEncoded();
    }

    public byte[] getSerialNumber() {
	    return _certificate.getSerialNumber().toString().getBytes();
    }

    public String getIssuerDN() {
	    return _certificate.getIssuerDN().getName();
    }

    public String getSubjectDN() {
	    return _certificate.getSubjectDN().getName();
    }

    public int getVersion() {
	    return _certificate.getVersion() -1;
    }

    public long getNotBefore() {
	    return (_certificate.getNotBefore().getTime())/1000;
    }

    public long getNotAfter() {
	    return (_certificate.getNotAfter().getTime())/1000;
    }

    public String toString() {
	    String str = "Certificate  :";
	    str += _certificate.toString();
	    return str;
    }

    private X509Certificate _certificate;
}
