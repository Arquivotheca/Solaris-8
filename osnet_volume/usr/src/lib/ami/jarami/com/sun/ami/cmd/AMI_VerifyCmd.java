/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_VerifyCmd.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.cmd;

import java.lang.*;
import java.text.*;
import java.util.*;
import java.io.*;
import java.rmi.*;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.security.*;
import java.security.cert.*;
import java.security.cert.Certificate;
import java.security.KeyPair;
import java.security.spec.*;
import java.math.BigInteger;
import sun.misc.BASE64Decoder;
import sun.security.pkcs.*;
import sun.security.x509.AlgorithmId;
import sun.security.x509.*;
import sun.security.util.*;

import com.sun.ami.AMI_Exception;
import com.sun.ami.utils.AMI_C_Utils;
import com.sun.ami.keymgnt.AMI_KeyMgntClient;
import com.sun.ami.common.SunAMI;

import com.sun.ami.keygen.AMI_PrivateKey;
import com.sun.ami.keygen.AMI_VirtualHost;
import com.sun.ami.utils.AMI_C_Utils;
import com.sun.ami.utils.AMI_GetOpt;
import com.sun.ami.common.*;
import com.sun.ami.ca.*;


/**
 * This class implements amisign command.
 * @author Mike Hsu
 *
 * @version 1.0
 *
 */

public class AMI_VerifyCmd extends AMI_Common {

    public AMI_VerifyCmd() {
	try {
            hostIP = InetAddress.getLocalHost().getHostAddress();
	} catch (UnknownHostException e) {
	    prtError(messages.getString("AMI_Cmd.host"));
	    exit(1);
	}
    }

    /**
     * Main method
     */
    public static void main(String[] argv)  {
	AMI_VerifyCmd verify = new AMI_VerifyCmd();

    	initialize();

	try {
	    verify.parseOption(argv);
	    verify.run();
	} catch (Exception e) {
	    System.err.println(e.getMessage());
	    verify.exit(1);
	}
    }

    //
    // Parse command line options
    //
    private void parseOption(String[] argv) throws Exception {

        AMI_GetOpt go = new AMI_GetOpt(argv, "hL:vseo:i:d:");
        int ch = -1;
	boolean isVirtualHost = false;
	String filename = null;

	/**
	 * Parse and take action for each specified property.
	 */

	while ((ch = go.getopt()) != go.optEOF) {

		if ((char)ch == 'd') {
		    try {
		        filename = go.optArgGet();
			File largef = new File(filename);
			int size = (int) largef.length();
			if (largef.length() != (long) size) {
			    prtError(messages.getString(
			        "AMI_Cmd.nolargefilesupport"));
			    exit (1);
			}
		        _dataStream = new FileInputStream(filename);
	   	    } catch (FileNotFoundException e) {
		        Object[] args = { new String(filename) };
		        msgFormatter.applyPattern(
			  messages.getString("AMI_Cmd.fileNotFound"));
		        prtError(msgFormatter.format(args));
		        exit(1);
		    }
	      	} else
	      	if ((char)ch == 'v') 
		    _verboseMode = true;
	      	else
	      	if ((char)ch == 's')
		    _silentMode = true;
	      	else
	      	if ((char)ch == 'i') {
		    try {
		          filename = go.optArgGet();
			  File largef = new File(filename);
			  int size = (int) largef.length();
			  if (largef.length() != (long) size) {
			      prtError(messages.getString(
			          "AMI_Cmd.nolargefilesupport"));
			      exit (1);
			  }
		          _sigStream = new FileInputStream(filename);
		    } catch (FileNotFoundException e) {
		          Object[] args = { new String(filename) };
			  msgFormatter.applyPattern(
				 messages.getString("AMI_Cmd.fileNotFound"));
			  prtError(msgFormatter.format(args));
			  exit(1);
		    }
	      	} else
	      	if ((char)ch == 'e') 
		    _printData = true;
	      	else
	      	if ((char)ch == 'o') {
		    try {
		          filename = go.optArgGet();
			  _outStream = new FileOutputStream(filename);
		    } catch (FileNotFoundException e) {
		        Object[] args = { new String(filename) };
		        msgFormatter.applyPattern(
			    messages.getString("AMI_Cmd.fileNotFound"));
		        prtError(msgFormatter.format(args));
		        exit(1);
		    } catch (IOException e) {
	    	        Object[] args = { new String(filename) };
	    	        msgFormatter.applyPattern(
			    messages.getString("AMI_Cmd.io"));
	    	        prtError(msgFormatter.format(args));
		        exit(1);
		    }
	    	} else
	    	if ((char)ch == 'h') {
		    if (!System.getProperties().
		    getProperty("user.name").equals("root")) {
		    prtError(messages.getString("AMI_Cmd.verify.rootonly"));
		    exit(1);
		}
		_type = AMI_Constants.AMI_HOST_OBJECT;
		if (!isVirtualHost)
		      _name = InetAddress.getLocalHost().getHostName();
	    	} else
	    	if ((char)ch == 'L') {
		    String virtual = go.optArgGet();
		    if (virtual.indexOf(".") < 0) {
		    	_name = virtual;
			hostIP = InetAddress.getByName(_name).getHostAddress();
		    } else {
			hostIP = virtual;
			try {
			_name = InetAddress.getByName(hostIP).getHostName();
			} catch (Exception e) {
			  prtError(messages.getString(
			  "AMI_Cmd.verify.invalidIP"));
			  exit(1);
			}
		    }
		    AMI_VirtualHost.setHostIP(hostIP);
		    isVirtualHost = true;
	    	} else 
			Usage();
	} // while

	/*
	* A root user has to explicitly specify - h to run.
	*/
	if (System.getProperties().getProperty("user.name").equals("root") && 
	    _type != AMI_Constants.AMI_HOST_OBJECT) {
	    prtError(messages.getString("AMI_Cmd.verify.isrootuser"));
	    exit(1);
	}
    }

    private void Usage() {
	prtError(messages.getString("AMI_Cmd.verify.usage"));
	exit(1);
    }

    private void verboseMessage(String msg) {
	if (_verboseMode)
	    System.out.println(msg);
    }

    private void prtMessage(String msg) {
	if (!_silentMode)
	    System.out.println(msg);
    }

    private void prtError(String msg) {
	if (!_silentMode)
	    System.err.println(msg);
    }

    private void exit(int err) {
	if (_silentMode)
	    err = 0;
	System.exit(err);
    }

    private void run() throws Exception {
	byte[] toBeParsed = null;
	byte[] dataArray;
	int nread;
	AMI_PKCS7 pkcs7;
	X509Certificate[] certchain;
	PublicKey pubkey;
	String SubjectName;
	String IssuerName;

	// Set the providers, if not already installed.
	if (Security.getProvider("SunAMI") == null) {
		   Security.insertProviderAt(new SunAMI(), 1);
	}

	//
	// Construct an PKCS7 object from the input file
	//
	toBeParsed = inport(_sigStream);
	try {
		pkcs7 = new AMI_PKCS7(toBeParsed);
	} catch (Exception e) {
		throw new Exception(messages.
		getString("AMI_Cmd.verify.corruptfile"));
	}

	//
	// Extract cert, issuer, publick key, and digest algo
	//
	certchain = pkcs7.getCertificates();
	SubjectName = certchain[0].getSubjectDN().getName();
	pubkey = certchain[0].getPublicKey();
	_digestAlgoId = pkcs7.getDigestAlgorithmIds();

	//
	// Check validity of the cert
	//
	if (!validateCertificateChain(certchain)) {
	    prtError(messages.getString("AMI_Cmd.verify.verifail"));
	    exit(1);
	}

	//
	try {
	    _signature = Signature.getInstance(_digestAlgoId[0].getName());
	} catch (NoSuchAlgorithmException e) {
	    Object[] args = { new String(_digestAlgoId[0].getName()) };
	    msgFormatter.applyPattern(
		messages.getString("AMI_Cmd.verify.algorithm"));
	    prtError(msgFormatter.format(args));
	    exit(1);
	}
	_signature.initVerify(pubkey);

	if (_dataStream == null) {
	    dataArray = pkcs7.getContentInfo().getData();
	} else {
	    dataArray = inputStreamToByteArray(_dataStream);
	}

	if (dataArray == null) {
	    prtError(messages.getString("AMI_Cmd.verify.nodata"));
	    exit(1);
	}

	_signature.update(dataArray);

	if (!_signature.verify(
	    pkcs7.getSignerInfos()[0].getEncryptedDigest())) {
	    prtMessage(messages.getString("AMI_Cmd.verify.vrfyFailed"));
	    System.exit(1);
	}

	// Verified ok.
	prtMessage(messages.getString("AMI_Cmd.verify.signedBy"));
	prtMessage(SubjectName);
	prtMessage(messages.getString("AMI_Cmd.verify.vrfyOk"));

	if (_printData)
	    _outStream.write(dataArray);
    }

    private boolean validateCertificateChain(X509Certificate[] certchain) {
	String Issuer;
	String Subject;
	X509Certificate cert;
	PublicKey pubkey;

	for (int ii = 0; ii < certchain.length; ii++) {
	    cert = certchain[ii];
	    //
	    // Check validity of the cert
	    //
	    try {
	        cert.checkValidity();
	    } catch (CertificateExpiredException e) {
	        prtError(messages.getString("AMI_Cmd.verify.expire"));
	        exit(1);
	    } catch (CertificateNotYetValidException e) {
	        prtError(messages.getString("AMI_Cmd.verify.nvalid"));
	        exit(1);
	    }

	    Issuer = cert.getIssuerDN().getName();
	    Subject = cert.getSubjectDN().getName();

	    verboseMessage("Issuer: " + Issuer);

	    /*
	     * If self signed certificate, accept with warning.
	     */
	    if (Subject.equals(Issuer)) {
		prtMessage(messages.getString("AMI_Cmd.verify.selfSignCert"));
		return true;
	    }

	    /*
	     * Get public key either from the next cert in chain,
	     * or, if at the end of chain, from the trusted public
	     * key in keystore.
	     */
	    if (ii < certchain.length - 1)
		pubkey = certchain[ii+1].getPublicKey();
	    else {
		if ((pubkey = getTrustedPublicKey()) == null) {
		    prtError(messages.getString("AMI_Cmd.verify.noTrustCert"));
		    return false;
		}
	    }

	    try {
		cert.verify(pubkey);
	    } catch (Exception e) {
		return false;
	    }
	}
	return true;
    }

    /*
     * Get public key off the trusted certificate in keystore.
     */
    private PublicKey getTrustedPublicKey() {
	KeyStore _keystore = null;
	Enumeration em;
	String alias;
	/*
	 * Get keystore
	 */
	try {
	    _keystore = KeyStore.getInstance("amiks", "SunAMI");
	    _keystore.load(null, null);
	    em = _keystore.aliases();
	    while (em.hasMoreElements()) {
		alias = (String) em.nextElement();
	        if (_keystore.isCertificateEntry(alias)) {
		    verboseMessage("Find trusted certificate." + alias);
		    return _keystore.getCertificate(alias).getPublicKey();
		}
	    }
	} catch (Exception e) {
	    prtError(e.getMessage());
	    // e.printStackTrace();
	} 
	return null;
    }

    /*
     * Load data from an inputStream to an byte array.
     */
    private byte[] inputStreamToByteArray(InputStream _in)
    throws IOException
    {
	byte[] _data = null;
	int	nread;
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
		for (int ii = 0; ii < last_idx; ii++)
		    buffer[ii] = _data[ii];
	    } else
	        buffer = new byte[nread];

	    for (int ii = 0; ii < nread; ii++)
		buffer[ii + last_idx] = buf[ii];

	    _data = buffer;
	}
	return _data;
    }

    private byte[] inport(InputStream inStream) throws IOException {
        StringBuffer    line = new StringBuffer();
        String          Sline;
        int             rc;
	final String Marker = "-----";
        BufferedReader  myBufferReader;
        BASE64Decoder base64 = new BASE64Decoder();

        myBufferReader = new BufferedReader
                        (new InputStreamReader(inStream));
/* 
Ajay Sondhi : 12/14/1998
If the length of the input line is less than Marker.length(), 
there is an error "String index out of range". To avoid it,
check if the input line length is less than or equal to Marker.length()
then just let it go 
*/

        while ((Sline = myBufferReader.readLine()) != null) {
            if (((Sline.length() <= Marker.length())) || 
	        (Sline.substring(0, Marker.length()).compareTo(Marker) != 0))
                line.append(Sline);
	}
        myBufferReader.close();
        return base64.decodeBuffer(line.toString());
    }

    /**
     * Initialize internationalization.
     */
    private static void initialize() {

	try {
       	    Locale currentLocale = AMI_Constants.initLocale();
       	    msgFormatter = new MessageFormat("");
       	    msgFormatter.setLocale(currentLocale);
       	    messages = AMI_Constants.getMessageBundle(
		currentLocale);
     	} catch (Exception e) {
	    System.out.println(e.getMessage());
	    e.printStackTrace();
     	}
    }


    // Internationalization
    private static ResourceBundle   messages;
    private static MessageFormat    msgFormatter;
    
    private boolean _addSignatureBoundry = false;	// -b
    private boolean _verboseMode = false;		// -v
    private boolean _silentMode = false;		// -s
    private boolean _printData = false;			// -e option
    private InputStream _sigStream = (InputStream) System.in;
    private InputStream _dataStream = (InputStream) null;
    private OutputStream _outStream = (OutputStream) System.out;
    private Signature _signature;
    private X509Certificate[] _cert = new X509Certificate[1];
    private String hostIP = null;
    private String _name = System.getProperties().getProperty("user.name");
    private String _type = AMI_Constants.AMI_USER_OBJECT;
    private AlgorithmId[] _algoId = new AlgorithmId[1];
    private AlgorithmId[] _digestAlgoId;
    private AlgorithmId _digestEncryptAlgoId;
}
