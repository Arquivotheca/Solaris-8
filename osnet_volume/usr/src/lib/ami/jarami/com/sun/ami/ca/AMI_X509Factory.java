/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_X509Factory.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.ca;

import java.io.*;
import java.math.BigInteger;
import java.util.Collection;
import java.util.Arrays;
import java.util.ArrayList;
import java.security.cert.CertificateFactorySpi;
import java.security.cert.Certificate;
import java.security.cert.CRL;
import java.security.cert.X509Certificate;
import java.security.cert.X509CRL;
import java.security.cert.CertificateException;
import java.security.cert.CRLException;
import sun.misc.BASE64Decoder;

/**
 * This class defines a certificate factory for X.509 v3 certificates.
 *
 * @author Sangeeta Varma
 *
 */

public class AMI_X509Factory extends CertificateFactorySpi {

    public static final String BEGIN_CERT = "-----BEGIN CERTIFICATE-----";
    public static final String END_CERT = "-----END CERTIFICATE-----";

    private static int defaultExpectedLineLength = 80;

    /**
     * Generates an X.509 certificate object and initializes it with
     * the data read from the input stream <code>is</code>.
     *
     * @param is an input stream with the certificate data.
     *
     * @return an X.509 certificate object initialized with the data
     * from the input stream.
     *
     * @exception CertificateException on parsing errors.
     */
    public Certificate engineGenerateCertificate(InputStream is)
	throws CertificateException
    {
	if (is == null) {
	    throw new CertificateException("Missing input stream");
	}
	try {
	    if (is.markSupported() == false) {
		// consume the entire input stream
		byte[] totalBytes;
		totalBytes = getTotalBytes(new BufferedInputStream(is));
		is = new ByteArrayInputStream(totalBytes);
	    };
	    // determine if binary or Base64 encoding. If Base64 encoding,
	    // the certificate must be bounded at the beginning by
	    // "-----BEGIN".
	    if (isBase64(is)) {
		// Base64
		byte[] data = base64_to_binary(is);
		return new AMI_X509CertImpl(data);
	    } else {
		// binary
		return new AMI_X509CertImpl(is);
	    }
	} catch (IOException ioe) {
	    throw new CertificateException(ioe.getMessage());
	}
    }

    /**
     * Returns a (possibly empty) collection view of X.509 certificates read
     * from the given input stream <code>is</code>.
     *
     * @param is the input stream with the certificates.
     *
     * @return a (possibly empty) collection view of X.509 certificate objects
     * initialized with the data from the input stream.
     *
     * @exception CertificateException on parsing errors.
     */
    public Collection engineGenerateCertificates(InputStream is)
        throws CertificateException
    {
	if (is == null) {
	    throw new CertificateException("Missing input stream");
	}
	try {
	    if (is.markSupported() == false) {
		// consume the entire input stream
		byte[] totalBytes;
		totalBytes = getTotalBytes(new BufferedInputStream(is));
		is = new ByteArrayInputStream(totalBytes);
	    };
	    // determine if binary or Base64 encoding. If Base64 encoding,
	    // the certificate must be bounded at the beginning by
	    // "-----BEGIN".
	    if (isBase64(is)) {
		// Base64
		byte[] data = base64_to_binary(is);
		return parseX509orPKCS7Cert(new ByteArrayInputStream(data));
	    } else {
		// binary
		return parseX509orPKCS7Cert(is);
	    }
	} catch (IOException ioe) {
	    throw new CertificateException(ioe.getMessage());
	}
    }

    public CRL engineGenerateCRL(InputStream is)
        throws CRLException
    {
         throw new CRLException("AMI_X509Factory::CRL not supported");
    }

    public Collection engineGenerateCRLs(InputStream is)
        throws CRLException
    {
         throw new CRLException("AMI_X509Factory::CRL not supported");
    }

    /*
     * Parses the data in the given input stream as an X.509
     * certificate or PKCS#7 formatted data.
     */
    private Collection parseX509orPKCS7Cert(InputStream is)
	throws CertificateException
    {
	try {
	    // treat as X.509 cert
	    is.mark(is.available());
	    AMI_X509CertImpl cert = new AMI_X509CertImpl(is);
	    return Arrays.asList(new X509Certificate[] { cert });
	} catch (CertificateException e) {
	    // treat as PKCS#7
	    try {
		is.reset();
		AMI_PKCS7 pkcs7 = new AMI_PKCS7(is);
		X509Certificate[] certs = pkcs7.getCertificates();
		// certs are optional in PKCS #7
		if (certs != null) {
		    return Arrays.asList(certs);
		} else {
		    // no certs provided
		    return new ArrayList(0);
		}
	    } catch (IOException ioe) {
		throw new CertificateException(ioe.getMessage());
	    }
	} catch (IOException ioe) {
	    throw new CertificateException(ioe.getMessage());
	}
    }


    /*
     * Converts a Base64-encoded X.509 certificate or X.509 CRL or PKCS#7 data
     * to binary encoding.
     * In all cases, the data must be bounded at the beginning by
     * "-----BEGIN", and must be bounded at the end by "-----END".
     */
    private byte[] base64_to_binary(InputStream is)
	throws IOException
    {
	long len = 0; // total length of base64 encoding, including boundaries

	is.mark(is.available());

	BufferedInputStream bufin = new BufferedInputStream(is);
	BufferedReader br = new BufferedReader(new InputStreamReader(bufin));

	// First read all of the data that is found between
	// the "-----BEGIN" and "-----END" boundaries into a buffer.
	String temp;
	if ((temp = readLine(br)) == null || !temp.startsWith("-----BEGIN")) {
	    throw new IOException("Unsupported encoding");
	} else {
	    len += temp.length();
	}
	StringBuffer strBuf = new StringBuffer();
	while ((temp = readLine(br)) != null && !temp.startsWith("-----END")) {
	    strBuf.append(temp);
	}
	if (temp == null) {
	    throw new IOException("Unsupported encoding");
	} else {
	    len += temp.length();
	}

	// consume only as much as was needed
	len += strBuf.length();
	is.reset();
	is.skip(len);

	// Now, that data is supposed to be a single X.509 certificate or
	// X.509 CRL or PKCS#7 formatted data... Base64 encoded.
	// Decode into binary and return the result.
	BASE64Decoder decoder = new BASE64Decoder();
	return decoder.decodeBuffer(strBuf.toString());
    }

    /*
     * Reads the entire input stream into a byte array.
     */
    private byte[] getTotalBytes(InputStream is) throws IOException {
	byte[] buffer = new byte[8192];
	ByteArrayOutputStream baos = new ByteArrayOutputStream(2048);
	int n;
	baos.reset();
	while ((n = is.read(buffer, 0, buffer.length)) != -1) {
	    baos.write(buffer, 0, n);
	}
	return baos.toByteArray();
    }

    /*
     * Determines if input is binary or Base64 encoded.
     */
    private boolean isBase64(InputStream is) throws IOException {
	if (is.available() >= 10) {
	    is.mark(10);
	    int c1 = is.read();
	    int c2 = is.read();
	    int c3 = is.read();
	    int c4 = is.read();
	    int c5 = is.read();
	    int c6 = is.read();
	    int c7 = is.read();
	    int c8 = is.read();
	    int c9 = is.read();
	    int c10 = is.read();
	    is.reset();
	    if (c1 == '-' && c2 == '-' && c3 == '-' && c4 == '-'
		&& c5 == '-' && c6 == 'B' && c7 == 'E' && c8 == 'G'
		&& c9 == 'I' && c10 == 'N') {
		return true;
	    } else {
		return false;
	    }
	} else {
	    throw new IOException("Cannot determine encoding format");
	}
    }

    /*
     * Read a line of text.  A line is considered to be terminated by any one
     * of a line feed ('\n'), a carriage return ('\r'), or a carriage return
     * followed immediately by a linefeed.
     *
     * @return     A String containing the contents of the line, including
     *             any line-termination characters, or null if the end of the
     *             stream has been reached.
     */
    private String readLine(BufferedReader br) throws IOException {
	int c;
	StringBuffer sb = new StringBuffer(defaultExpectedLineLength);
	do {
	    c = br.read();
	    sb.append((char)c);
	} while (c != -1 && c != '\n' && c != '\r');
	if (c == -1) {
	    return null;
	}
	if (c == '\r') {
	    br.mark(1);
	    int c2 = br.read();
	    if (c2 == '\n') {
		sb.append((char)c);
	    } else {
		br.reset();
	    }
	}
	return sb.toString();
    }
}
