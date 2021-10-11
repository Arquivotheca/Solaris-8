/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_SignCmd.java	1.1 99/07/11 SMI"
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
import com.sun.ami.keymgnt.AMI_KeyMgntClient;
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

public class AMI_SignCmd extends AMI_Common {

    public AMI_SignCmd() {
	try {
            hostIP = InetAddress.getLocalHost().getHostAddress();
	} catch (UnknownHostException e) {
	    if (!_silentMode)
	      	System.err.println(messages.getString("AMI_Cmd.host"));
	    exit(1);
	}
    }

    /**
     * Main method
     */
    public static void main(String[] argv)  {
	AMI_SignCmd sign = new AMI_SignCmd();

    	initialize();

	try {
	    sign.parseOption(argv);
	    sign.run();
	} catch (Exception e) {
	    sign.prtError(e.getLocalizedMessage());
	    sign.exit(1);
	}
    }

    //
    // Parse command line options
    //
    private void parseOption(String[] argv) throws Exception {
            
        AMI_GetOpt go = new AMI_GetOpt(argv, "bcvsxk:a:i:o:hL:");
        int ch = -1;
	boolean isVirtualHost = false;
	String filename = null;

	while ((ch = go.getopt()) != go.optEOF) {

		if ((char)ch == 'b') 
		    _prtSignatureBoundry = true;
	      	else 
	      	if ((char)ch == 'c')
		    _userCertificateOnly = true;
	      	else 
	      	if ((char)ch == 'v')
		    _verboseMode = true;
	      	else
	      	if ((char)ch == 's')
		    _silentMode = true;
	      	else
	      	if ((char)ch == 'x')
		    _onlyPrintSignature = true;
	      	else
	      	if ((char)ch == 'k')
		_algorithm = go.optArgGet();
	      	else
	      	if ((char)ch == 'a')
		_keyalias =  go.optArgGet();
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
		    _input = new FileInputStream(filename);
		} catch (FileNotFoundException e) {
		    Object[] args = { new String(filename) };
		    msgFormatter.applyPattern(
			messages.getString("AMI_Cmd.fileNotFound"));
		    prtError(msgFormatter.format(args));
		    exit(1);
                }
	    } else
	    if ((char)ch == 'o') {
		try {
		    filename = go.optArgGet();
		    _output = new FileOutputStream(filename);
		} catch (Exception e) {
		    Object[] args = { new String(filename) };
		    msgFormatter.applyPattern(
			messages.getString("AMI_Cmd.sign.cantaccessfile"));
		    prtError(msgFormatter.format(args));
		    exit(1);
		} 
	    } else
	    if ((char)ch == 'h') {
		if (!_loginName.equals("root")) {
		    prtError(messages.getString("AMI_Cmd.sign.rootonly"));
		    exit(1);
	        }
	        _type = AMI_Constants.AMI_HOST_OBJECT;
	    	// If -L option is not found before this
		// set name to current host
	    	// otherwise use name set in the -L option
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
			_name = InetAddress.getByName(hostIP).getHostName();
		    }
		    AMI_VirtualHost.setHostIP(hostIP);
		    isVirtualHost = true;
	    } else {
		    Usage();
		    exit(1);
	    }
	}

	/*
	 * Check if algo is valid
	 */
	if (!algorithmSupported(_algorithm)) {
	    Object[] args = { new String(_algorithm) };
	    msgFormatter.applyPattern(
		messages.getString("AMI_Cmd.sign.algorithm"));
	    prtError(msgFormatter.format(args));
	    exit(1);
	}
	/*
	 * A root user has to explicitly specify -h to run.
	 */
	if (_loginName.equals("root") && 
	    _type != AMI_Constants.AMI_HOST_OBJECT) {
	    prtError(messages.getString("AMI_Cmd.sign.isrootuser"));
	    exit(1);
	}
	/*
	 * If -L is specified, then -h has to be specified 
	 */
	if (isVirtualHost && 
	    _type != AMI_Constants.AMI_HOST_OBJECT) {
	    Usage();
	    exit(1);
	}
	/*
	 * Set keyalias
	 */
	if (_keyalias == null) {
	    if (_algorithm.equalsIgnoreCase("MD5withRSA") ||
	        _algorithm.equalsIgnoreCase("MD5/RSA")	  ||
		_algorithm.equalsIgnoreCase("MD2withRSA") ||
	    	_algorithm.equalsIgnoreCase("MD2/RSA")) {

	    	_keyalias = AMI_KeyMgntClient.getRSASignAlias(_name,
				AMI_Constants.AMI_USER_OBJECT);
	    } else
	    if (_algorithm.equalsIgnoreCase("SHA1withDSA") ||
	        _algorithm.equalsIgnoreCase("SHA1/DSA")) {

	    	_keyalias = AMI_KeyMgntClient.getDSAAlias(_name,
				AMI_Constants.AMI_USER_OBJECT);
	    }
        }
	if (_keyalias == null)
	    _keyalias = "mykey";

    }

    private void Usage() {
	prtError(messages.getString("AMI_Cmd.sign.usage"));
	exit(-1);
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
	byte[] toBeSigned = null;
	byte[] buf = new byte[1024];
	int nread;
	AMI_PKCS7 pkcs7;
	SignerInfo[] signer = new SignerInfo[1];

	// Set the providers, if not already installed.
	if (Security.getProvider("SunAMI") == null) {
		   Security.insertProviderAt(new SunAMI(), 1);
	}

	/*
	 * Get keystore
	 */
	try {
	    _keystore = KeyStore.getInstance("amiks", "SunAMI");
	} catch (NoSuchProviderException e) {
	    Object[] args = { new String(e.getMessage()) };
	    msgFormatter.applyPattern(
		messages.getString("AMI_Cmd.sign.provider"));
	    prtError(msgFormatter.format(args));
	    exit(1);
	} catch (KeyStoreException e) {
	    prtError(e.getMessage());
	    // e.printStackTrace();
	    exit(1);
	}

	_keystore.load(null, null);

	// Get private key and certificate
	if (!_keyalias.equals(null)) {
	    _privkey = (PrivateKey) _keystore.getKey(_keyalias, null);
	    _cert[0] = (X509Certificate)_keystore.getCertificate(_keyalias);
	}

	if (_privkey == null) {
	    Object[] args = { new String(_keyalias) };
	    msgFormatter.applyPattern(messages.getString("AMI_Cmd.sign.key"));
	    prtError(msgFormatter.format(args));
	    exit(1);
	}

	/*
	* PK: 2/1/99:  Check that certificate is valid before signing 
	*/
       	try {
     	  _cert[0].checkValidity();
       	} catch (CertificateExpiredException e) {

          Object[] args = { new String(_keyalias) };
          msgFormatter.applyPattern( 
   	     		messages.getString("AMI_Cmd.sign.certExpired"));
          prtError(msgFormatter.format(args));
          exit(1);

       	} catch (CertificateNotYetValidException e) {

          Object[] args = { new String(_keyalias) };
          msgFormatter.applyPattern( 
	     		messages.getString("AMI_Cmd.sign.certNotYetValid"));
          prtError(msgFormatter.format(args));
          exit(1);

       	} catch (Exception e) {

         Object[] args = { new String(_keyalias) };
         msgFormatter.applyPattern( 
	     		messages.getString("AMI_Cmd.sign.certErrValidating"));
         prtError(msgFormatter.format(args));
         exit(1);

        }

	//
	// Read file into toBeSigned array
	//
	while ((nread = _input.read(buf)) > 0) {
	    byte[] buffer = null;
	    int last_idx = 0;

	    if (toBeSigned != null) {
	        buffer = new byte[nread + toBeSigned.length];
		last_idx = toBeSigned.length;
		for (int ii = 0; ii < last_idx; ii++)
		    buffer[ii] = toBeSigned[ii];
	    } else
	        buffer = new byte[nread];

	    for (int ii = 0; ii < nread; ii++)
		buffer[ii + last_idx] = buf[ii];

	    toBeSigned = buffer;
	}

	// 
	try {
	    if (_verboseMode)
		System.out.println(_algorithm);
	    _signature = Signature.getInstance(_algorithm, "SunAMI");
	} catch (NoSuchAlgorithmException e) {
	    Object[] args = { new String(_algorithm) };
	    msgFormatter.applyPattern(
		messages.getString("AMI_Cmd.sign.algorithm"));
	    prtError(msgFormatter.format(args));
	    exit(1);
	}
        if (toBeSigned == null) {
            prtError(messages.getString("AMI_Cmd.sign.nodata"));
	    exit(1);
	}
	_signature.initSign(_privkey);
	if (_verboseMode)
        	System.out.println(
		"_signature :"+ _signature + "tobesigned: "+toBeSigned);
	_signature.update(toBeSigned);

	// Set up SignerInfo
	signer[0] = new SignerInfo(
			new X500Name(AMI_KeyMgntClient.getDNNameFromLoginName(
				_name, _type)),
			new BigInt(_cert[0].getSerialNumber()),
			_digestAlgoId,
			_digestEncryptAlgoId,
			_signature.sign());

	//
	if (_onlyPrintSignature) {
	    pkcs7 = new AMI_PKCS7(_algoId, 
	    	new AMI_ContentInfo(AMI_ContentInfo.DATA_OID, (DerValue)null),
	    	_cert, signer);
	    pkcs7.printSignatureOnly(new PrintStream(_output),
	    		_prtSignatureBoundry);
	} else {
	    pkcs7 = new AMI_PKCS7(_algoId,
		new AMI_ContentInfo(toBeSigned), _cert, signer);
	    pkcs7.printSignatureAndData(new PrintStream(_output),
		_prtSignatureBoundry);
	}
    }

    private boolean algorithmSupported(String algo) {
	if (algo.equalsIgnoreCase("MD5withRSA") ||
	    algo.equalsIgnoreCase("MD5/RSA")) {
	    _algoId[0] = new AlgorithmId(AlgorithmId.md5WithRSAEncryption_oid);
	    _digestAlgoId = new AlgorithmId(AlgorithmId.MD5_oid);
	    _digestEncryptAlgoId = new AlgorithmId(AlgorithmId.RSA_oid);
	} else
	if (algo.equalsIgnoreCase("MD2withRSA") ||
	    algo.equalsIgnoreCase("MD2/RSA")) {
	    _algoId[0] = new AlgorithmId(AlgorithmId.md2WithRSAEncryption_oid);
	    _digestAlgoId = new AlgorithmId(AlgorithmId.MD2_oid);
	    _digestEncryptAlgoId = new AlgorithmId(AlgorithmId.RSA_oid);
	} else
	if (algo.equalsIgnoreCase("SHA1withDSA") ||
	    algo.equalsIgnoreCase("SHA1/DSA")) {
	    _algoId[0] = new AlgorithmId(AlgorithmId.sha1WithDSA_oid);
	    _digestAlgoId = new AlgorithmId(AlgorithmId.SHA_oid);
	    _digestEncryptAlgoId = new AlgorithmId(AlgorithmId.DSA_oid);
	} else
	    return false;
	return true;
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
	    // e.printStackTrace();
     	}
    }

    // Internationalization
    private static ResourceBundle   messages;
    private static MessageFormat    msgFormatter;

    private boolean _prtSignatureBoundry = false;
    private boolean _userCertificateOnly = false;
    private boolean _verboseMode = false;
    private boolean _silentMode = false;
    private boolean _onlyPrintSignature = false;
    private String _algorithm = "MD5withRSA";
    private String _keyalias = null;
    private PrivateKey _privkey = null;
    private InputStream _input = (InputStream) System.in;
    private OutputStream _output = (OutputStream) System.out;
    private Signature _signature;
    private KeyStore _keystore;
    private X509Certificate[] _cert = new X509Certificate[1];
    private String hostIP = null;
    
    // This variable will have login name if it is a user , otherwise will be 
    // set to the host name if it is a host
    private String _name = System.getProperties().getProperty("user.name");
  
    // This variable will ALWAYS have login name
    private String _loginName = System.getProperties().getProperty("user.name");

    private String _type = AMI_Constants.AMI_USER_OBJECT;
    private AlgorithmId[] _algoId = new AlgorithmId[1];
    private AlgorithmId _digestAlgoId;
    private AlgorithmId _digestEncryptAlgoId;
}
