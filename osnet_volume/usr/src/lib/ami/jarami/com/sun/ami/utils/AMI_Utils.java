/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_Utils.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.utils;
import java.lang.*;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.InputStreamReader;
import java.io.InputStream;
import java.io.ByteArrayInputStream;
import java.io.IOException;

import java.text.MessageFormat;

import java.util.StringTokenizer;
import java.util.ResourceBundle;
import java.util.Vector;
import java.util.Enumeration;

import java.security.Security;

import sun.misc.BASE64Decoder;

import com.sun.ami.keymgnt.AMI_KeyMgntClient;
import com.sun.ami.common.AMI_Constants;
import com.sun.ami.common.AMI_Log;
import sun.security.x509.X509CertImpl;

/**
 *
 * The following Class contains common Java utilities which can be used
 * by any object after having cvreated a new instance of this class
 * Some of them are InputStreamToByteArray ( which reads any given input
 * stream reads,t completely and returns a byte array of data read
 * The other is LoadCert , which loads and returns an X509/PKCS7
 * cert from the input stream.
 * Please use this to add any additional java utils which you may foresee
 *
 * @author  Bhavna Bhatnagar
 *
 */

public class AMI_Utils {
	final static int ERROR = -1;

	/* Cmdline parser operates on these fields */
	static boolean verboseMode = false;
	static boolean silentMode = false;
	static byte binaryCert[];

	static ResourceBundle messages;
	static MessageFormat msgFormatter;

	static final String CERT_BEGIN = "-----BEGIN CERTIFICATE-----";
	static final String CERT_END = "-----END CERTIFICATE-----";
	/**
         * LOAD CERTIFICATE
         * The following method loads BASE64 DER encoded certificate
         * into a byte array.
         */

	public static byte[] LoadSingleCert(BufferedReader certIn)
	    throws Exception {
		String line = null;
		StringBuffer certBuf = new StringBuffer();
		BASE64Decoder base64 = new BASE64Decoder();

		/*
		 * Locate begin marker
		 */
		line = certIn.readLine();
		while ((line != null) && !line.equals(CERT_BEGIN)) {
			line = certIn.readLine();
		}
		if (line == null) {
			// End of file reached
			return (null);
		}

		/*
		 * Collect lines into a single buffer until reaching
		 * end marker
		 */
		line = certIn.readLine();
		while (line != null && !line.equals(CERT_END)) {
			certBuf.append(line);
			line = certIn.readLine();
		}

		if (line == null)
			abort(null, messages.getString(
			    "AMI_Cmd.cert.err_invalidcert"));

		/*
		 * Convert certificate request from BASE64 encoded to
		 * binary
		 */
		return (base64.decodeBuffer(certBuf.toString()));
	}
	/**
         * LOAD CERTIFICATES
         * The following method loads BASE64 DER encoded certificates
         * into a Vector from an inputFile
         */


public static Vector LoadCert(String inputFile, String certOwner,
    String certIssuer, long certSerialNbr) {
	BufferedReader certIn;
	byte binaryCert[] = null;
	Vector certs = new Vector();
	X509CertImpl certificate;

	// First determine the DN name user is looking for
	String certOwnerDN = null;
	if (certOwner == null) {
		certOwnerDN = null;
	} else if (certOwner.indexOf('=') == -1) {
		try {
			certOwnerDN = AMI_KeyMgntClient.getDNNameFromLoginName(
			    certOwner, AMI_Constants.AMI_USER_OBJECT);
			if (certOwnerDN == null) {
				certOwnerDN =
				    AMI_KeyMgntClient.getDNNameFromLoginName(
				    certOwner, AMI_Constants.AMI_HOST_OBJECT);
			}
			if (certOwnerDN == null) {
				// Unable to obtain the DN name
				return (null);
			}
		} catch (Exception e) {
			return (null);
		}
	} else {
		certOwnerDN = certOwner;
	}

	if (inputFile != null)
		 logmsg("AMI_Cmd.cert.loadcert");
	else
		 logmsg("AMI_Cmd.cert.loadcertStdin");

	try {
		/*
		 * Establish input source of certificate request
		 */

		if (inputFile != null)
			certIn = new BufferedReader(
			    new FileReader(inputFile));
		else
			certIn = new BufferedReader(
			    new InputStreamReader(System.in));

		if (certIn == null)
			abort(null, messages.getString(
			    "AMI_Cmd.cert.err_openinput"));

		while ((binaryCert = LoadSingleCert(certIn)) != null) {
			certificate = ParseCert(binaryCert);
			certs.addElement(certificate);
		}
		if (inputFile != null) {
			certIn.close();	
                // Memory mgt. should take care of the  reader
		}
	} catch (Exception e) {
		abort(e, messages.getString("AMI_Cmd.cert.err_certread"));
	}
	return (getRequiredCertificates(certs, certOwnerDN, certIssuer,
	    certSerialNbr));
}


public static Vector getRequiredCertificates(Vector inCerts, String certOwner,
    String certIssuer, long certSerialNbr) {
	Vector certs = new Vector(10);
	X509CertImpl certificate;

	if (certSerialNbr == 0 && certOwner == null && certIssuer == null) {
		// Request for all certificates
		return inCerts;
	}

	Enumeration inCert = inCerts.elements();
	boolean isMatch;
	String normalizedCertOwner = null, normalizedCertIssuer = null;
	if (certOwner != null)
		normalizedCertOwner = AMI_KeyMgntClient.normalizeDN(certOwner);
	if (certIssuer != null)
	    normalizedCertIssuer = AMI_KeyMgntClient.normalizeDN(certIssuer);
	while (inCert.hasMoreElements()) {
		isMatch = true;
		certificate = (X509CertImpl) inCert.nextElement();
		if ((certSerialNbr != 0) &&
		    (certificate.getSerialNumber().longValue() !=
		    certSerialNbr)) {
			isMatch = false;
		}
		
		if (isMatch && (certOwner != null) &&
		    !normalizedCertOwner.equals(AMI_KeyMgntClient.normalizeDN(
		    certificate.getSubjectDN().toString()))) {
			isMatch = false;
		}
		if (isMatch && (certIssuer != null) &&
		    !normalizedCertIssuer.equals(AMI_KeyMgntClient.normalizeDN(
		    certificate.getIssuerDN().toString()))) {
			isMatch = false;
		}

		if (isMatch)
			certs.addElement(certificate);
	}
	return (certs);
}

/**
* PARSE CERTIFICATE
* This method takes the binaryCert byte-array created when
* the KeyStore was loaded, and converts the certfificate data
* in to an X509Certificate object.
*/
public static X509CertImpl ParseCert(byte[] certBytes) {
	X509CertImpl theCert = null;
	ByteArrayInputStream certStream =
	    new ByteArrayInputStream(certBytes);

	try {
		theCert = (X509CertImpl) new X509CertImpl(certStream);
	} catch (Exception e) {
		abort(e, messages.getString("AMI_Cmd.cert.err_readcert"));
	}

	return theCert;
}

private static void logmsg(String key) {
	if (!silentMode && verboseMode)
		System.err.println(messages.getString(key));
}


/*
* . If the verbose flag is true and the exception parameter is not null,
*   a stack traceback is displayed.
*
* @parameter Exception - Exception to traceback if non-silent/verbose mode.
* @parameter String    - Message to display if non-silent mode set
*
*/
private static void abort(Exception e, String msg) {
	if (!silentMode) {
		System.err.println(msg);
		if (e != null && verboseMode) {
			e.printStackTrace();
		}
	}
	System.exit(ERROR);
}
/*
* Load data from an inputStream to an byte array.
*/
    public byte[] InputStreamToByteArray(InputStream _in)
    throws IOException
    {
        byte[] _data = null;
        int     nread;
        byte[] buf = new byte[1024];
        //
        // Read file into _data array
        //
        while ((nread = _in.read(buf)) > 0) {
            byte[] buffer = null;
            int last_idx = 0;

            if (_data != null) {
                buffer = new byte[nread + _data.length];
                last_idx = _data.length;
                for (int ii = 0; ii < last_idx; ii ++)
                    buffer[ii] = _data[ii];
            } else
                buffer = new byte[nread];

            for (int ii = 0; ii < nread; ii++)
                buffer[ii + last_idx] = buf[ii];

            _data = buffer;
        }
        return _data;
    }


}
