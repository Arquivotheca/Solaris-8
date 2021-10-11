/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_DecryptCmd.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.cmd;

import java.lang.*;
import java.text.*;
import java.util.*;
import java.io.*;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.security.*;
import java.security.cert.*;
import java.security.cert.Certificate;
import sun.misc.BASE64Decoder;
import sun.security.pkcs.*;
import sun.security.x509.X500Name;
import sun.security.util.BigInt;

import com.sun.ami.AMI_Exception;
import com.sun.ami.keymgnt.AMI_KeyMgntClient;
import com.sun.ami.keymgnt.AMI_KeyStore_Certs;
import com.sun.ami.keygen.AMI_VirtualHost;
import com.sun.ami.common.*;
import com.sun.ami.utils.AMI_C_Utils;
import com.sun.ami.utils.AMI_GetOpt;
import sun.security.util.DerInputStream;
import sun.security.util.DerOutputStream;
import sun.security.util.DerValue;
import sun.security.util.ObjectIdentifier;
import com.sun.ami.crypto.AMI_Crypto;
import com.sun.ami.crypto.AMI_CryptoException;
import com.sun.ami.ca.*;
import com.sun.ami.amiserv.*;

/**
 * This class implements amidecrypt command.
 *
 * @author Sangeeta
 * @author Bhavna 
 *
 * @version 1.0
 *
 */

public class AMI_DecryptCmd extends AMI_Common {

    private String DEFAULT_UNWRAP_ALGO = "RSA";
    private String DEFAULT_CIPHER_FILENAME = "cipherdata";
    private String DEFAULT_CIPHER_EXTN = ".cipherdata";
    private int AMI_END_DATA = 2;

    public AMI_DecryptCmd()  {

        AMI_C_Utils utils = new AMI_C_Utils();

	try {
	    _userId = utils.ami_get_user_id();

	    _hostIP = InetAddress.getLocalHost().getHostAddress();
	} catch (Exception e) {
	    System.out.println(e.getMessage());
	    exit(1);
	}

    }

    /*
     * Parse command line options
     */
    private void parseOption(String[] argv) throws Exception {
        AMI_GetOpt go = new AMI_GetOpt(argv, "svi:c:o:hL:");
        int ch = -1;
	boolean isVirtualHost = false;
	String filename = null;

	String inputFile = null;

	/**
	 * Parse and take action for each specified property.
	 */
        if (argv.length == 0) {
	    Usage();
	    exit(1);
	} 
	while ((ch = go.getopt()) != go.optEOF) {

	    if ((char)ch == 'c') {
	        _cipherFile = go.optArgGet();
		cipherFileSpecified = true;
	    }
	    else
	    if ((char)ch == 'h') {
		if (!System.getProperties().getProperty("user.name").
		equals("root")) {
		    prtError(messages.getString("AMI_Cmd.decrypt.rootonly"));
		    exit(1);
		}
		_type = AMI_Constants.AMI_HOST_OBJECT;
		if (!isVirtualHost)
		    _name = InetAddress.getLocalHost().getHostName();
	    }
	    else
	    if ((char)ch == 'L') {
		    String virtual = go.optArgGet();
		    if (virtual.indexOf(".") < 0) {
		        _name = virtual;
			_hostIP = InetAddress.getByName(virtual).
			getHostAddress();
		    } else {
			_hostIP = virtual;
			_name = InetAddress.getByName(_hostIP).getHostName();
		    }
		    AMI_VirtualHost.setHostIP(_hostIP);
		    isVirtualHost = true;
	    } 
	    else
	    if ((char)ch == 'i') {
		inputFile = go.optArgGet();
		try {
		    _fis = new FileInputStream(inputFile);
		} catch (FileNotFoundException e) {
		    Object[] args = { new String(inputFile) };
		    msgFormatter.applyPattern(
			messages.getString("AMI_Cmd.fileNotFound"));
		    prtError(msgFormatter.format(args));
		    exit(1);
		}

	    }
	    else
	    if ((char)ch == 'o') {
		try {
		    filename = go.optArgGet();
		    _fos = new FileOutputStream(filename);
		} catch (IOException e) {
	    	    Object[] args = { new String(filename) };
	    	    msgFormatter.applyPattern(
			messages.getString("AMI_Cmd.io"));
	    	    prtError(msgFormatter.format(args));
		    exit(1);
		}
	    } 
	    else
	    if ((char)ch == 's') 
		_silentMode = true;
	    else
	    if ((char)ch == 'v') 
		_verboseMode = true;
	    else {	      	
	        Usage();
	        exit(1);
	    }
	} 
	// while 

	if (_verboseMode)
	    System.out.println("decryptCmd:: Parsed all options");
	/*
	 * A root user has to explicitly specify -h to run.
	 */
	if (System.getProperties().getProperty("user.name").equals("root") 
	    && (_type != AMI_Constants.AMI_HOST_OBJECT)) {
	    prtError(messages.getString("AMI_Cmd.decrypt.isrootuser"));
	    exit(1);
	}

	if (!cipherFileSpecified && inputFile != null)
		    _cipherFile = inputFile + DEFAULT_CIPHER_EXTN;
    }

    private void run() throws Exception {

	KeyStore keystore = null;
	byte[] symmetricKey = null;
	byte[] wrappedKey = null;
	byte[] decryptedData = null;
	byte[] encryptedData = null;
	PrivateKey privkey = null;

	/* 
	* Read input file and PKCS7 parse it 
	*/
	AMI_PKCS7 pkcs7 = new AMI_PKCS7(readInputFile(_fis));

	// Check content
	// If null, read cipher file 
	
	encryptedData = getEncryptedData(pkcs7);
       
	wrappedKey = getWrappedKey(pkcs7);


	// Get our own keystore from server 
	// For each privatekey of type RSA, try and unwrap session key 

	if (_verboseMode)
	        System.out.println("decryptCmd:: Getting our keystore");

        keystore = KeyStore.getInstance("amiks", "SunAMI");
	keystore.load(null, null);

	Enumeration enum = keystore.aliases();

	while (enum.hasMoreElements()) {

	    String alias = (String)enum.nextElement();

	    if (keystore.isKeyEntry(alias)) {
		privkey = (PrivateKey) keystore.
		getKey(alias, null);		  
		if (!privkey.getAlgorithm().equals("RSA"))
		       continue;  // Not an RSA key, try next key

	  	try {
	      	    if ((symmetricKey = unwrapKey(wrappedKey, alias)) != null)
			    break;
		} catch (AMI_Exception e) {
                if (_verboseMode)
                    System.out.println(e.getMessage());
		}
	   }		    
	}

	if (symmetricKey == null)
	    throw new AMI_Exception("Unable to unwrap the key !");

	// Decrypt Data 
	if ((decryptedData = decrypt(symmetricKey,
	encryptedData, pkcs7)) == null)
	    throw new AMI_Exception("Null output data from decrypt!");

	// Write Output to appropriate files 
	 
	writeOutputFile(decryptedData);

    }

    private byte[] getWrappedKey(AMI_PKCS7 pkcs) throws Exception 
    {
        byte[] wrappedKey = null;
	X509Certificate x509cert = null;
	AMI_RecipientInfo[] recInfos = pkcs.getRecipientInfos();
	AMI_RecipientInfo myRecInfo = null;

	Certificate [] mycerts = _certsKeyStore.engineGetCertificates(_name);

        if (mycerts == null || mycerts.length == 0) {
		    Object[] args = { new String(_name) };
		    msgFormatter.applyPattern(
			messages.getString("AMI_Cmd.decrypt.noCert"));
		    throw new AMI_Exception(msgFormatter.format(args));
	}

	for (int ii = 0; ii < recInfos.length; ii++)
	{
	    for (int jj = 0; jj < mycerts.length; jj++) {
	        x509cert = (X509Certificate)mycerts[jj];
	        if (recInfos[ii].getIssuerName().equals(x509cert.
		getIssuerDN()) && recInfos[ii].getCertificateSerialNumber().
		toBigInteger().equals(x509cert.getSerialNumber()))
		    return recInfos[ii].getEncryptedKey();
	    }
        }
	
	throw new AMI_Exception(messages.
	getString("AMI_Cmd.decrypt.invalidRecList"));
    }

    private byte[] getEncryptedData(AMI_PKCS7 pkcs) throws AMI_Exception,
                                                 IOException
    {
        byte[] encryptedData = null;

        if ((encryptedData = pkcs.getEncryptedContentInfo().
	getContentBytes()) != null) {
	    return encryptedData;
        }

	// Otherwise  read cipher file
	 
	FileInputStream cis = new FileInputStream(_cipherFile);
        System.out.println(" No data, reading cipher file");

	return (readInputFile(cis));
    }
 

    private byte[] decrypt(byte[] symmetricKey, byte[] encryptedData,
    AMI_PKCS7 pkcs) throws Exception
    {
        AMI_EncryptedContentInfo encinfo = null;
	AlgorithmId aid = pkcs.getEncryptedContentInfo().
	getContentEncryptionAlgorithm();
	String algo = aid.getName();

        if (_verboseMode)
	    System.out.print("decryptCmd:: Decrypting the data using:  ");
	    System.out.println(aid.getName());

	// Call decrypt JNI 
	AMI_Crypto crypto = new AMI_Crypto();

	crypto.ami_init();
	// Decrypting
	if (algo.equalsIgnoreCase("RC4"))
	    crypto.ami_rc4_decrypt(encryptedData, encryptedData.length,
	    AMI_END_DATA, symmetricKey, symmetricKey.length);

	else if (algo.equalsIgnoreCase("RC2")) {
        	byte[] paramsBytes = aid.encode();
		DerInputStream dis = new DerInputStream(paramsBytes);
		DerValue[] data = dis.getSequence(2);
		DerInputStream oiddis = new 
		DerInputStream(data[0].toByteArray());
        	ObjectIdentifier oid = oiddis.getOID();
        	int _effectiveKeySize = data[1].data.getInteger().toInt();
        	byte[] _iv = data[1].data.getBitString();
	        crypto.ami_rc2_decrypt(encryptedData, encryptedData.length,
		AMI_END_DATA, symmetricKey, symmetricKey.length, 
				       _effectiveKeySize, _iv);

	} else if (algo.equalsIgnoreCase("DES") ||
		algo.equalsIgnoreCase("3DES")) {
		if (_verboseMode) {
		    System.out.println("into DES/3DES");
		}
		byte[] paramsBytes = aid.encode();
		DerInputStream dis = new DerInputStream(paramsBytes);
		DerValue[] data = dis.getSequence(2);
		DerInputStream oiddis = 
		new DerInputStream(data[0].toByteArray());
        	ObjectIdentifier oid = oiddis.getOID();
        	byte[] _iv = data[1].data.getOctetString();
	        crypto.ami_des3des_decrypt(algo.toUpperCase(), encryptedData,
		encryptedData.length, AMI_END_DATA, symmetricKey, _iv);
	} else
		throw new AMI_Exception(messages.getString(
		"AMI_Cmd.decrypt.unsupportedAlgo"));

	crypto.ami_end();

	  // Return the decrypted data
	  return crypto.getOutputData();
    }


    private byte[] readInputFile(InputStream inStream) throws IOException {
        StringBuffer    line = new StringBuffer();
        String          Sline;
        int             rc;
	final String Marker = "-----";
        BufferedReader  myBufferReader;
        BASE64Decoder base64 = new BASE64Decoder();

        myBufferReader = new BufferedReader(new InputStreamReader(inStream));
	// remove all Marker lines, but still include all lines shorter
	// than Marker lines ** Bhavna 

        while ((Sline = myBufferReader.readLine()) != null) {
	    if (Sline.length() >= Marker.length()) {
	        if (Sline.substring(0, Marker.length()).
		    compareTo(Marker) != 0) 
                    line.append(Sline);
	    }
	    else
		line.append(Sline);
	}
        myBufferReader.close();
        return base64.decodeBuffer(line.toString());
    }

    private byte[] unwrapKey(byte[] wrappedKey, String alias)
	throws AMI_Exception, AMI_CryptoException, IOException
    {
	  AMI_Crypto crypto = new AMI_Crypto();

          AMI_KeyServ client = AMI_KeyServClient.getInstance(
	  	AMI_KeyMgntClient.getProperty(AMI_Constants.PROTOCOL_PROPERTY));

          AMI_CryptoInfo info = new AMI_CryptoInfo(null, _hostIP, _userId, null,
          	wrappedKey, DEFAULT_UNWRAP_ALGO, alias);

	  // Return the unwrapped key
	  return client.unwrapData(info);
    }

    private void writeOutputFile(byte[] decryptedData) 
                                     throws Exception 
    {
          _fos.write(decryptedData);
    }


    private void Usage() {
	prtError(messages.getString("AMI_Cmd.decrypt.usage"));
	exit(1);
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

    /**
     * Initialize internationalization.
     */
    private static void initialize() throws Exception {

       	 Locale currentLocale = AMI_Constants.initLocale();
       	 msgFormatter = new MessageFormat("");
       	 msgFormatter.setLocale(currentLocale);
       	 messages = AMI_Constants.getMessageBundle(currentLocale);
	 _certsKeyStore = new AMI_KeyStore_Certs();
    }

    /**
     * Main method
     */
    public static void main(String[] argv)  {
	AMI_DecryptCmd decrypt = new AMI_DecryptCmd();

	try {
	    initialize();
	    decrypt.parseOption(argv);
	    decrypt.run();
	} catch (Exception e) {
	    System.out.println(e.getMessage());
	    decrypt.exit(1);
	}
    }

    // Internationalization
    private boolean cipherFileSpecified = false;
    private static ResourceBundle   messages;
    private static MessageFormat    msgFormatter;
    private static AMI_KeyStore_Certs _certsKeyStore = null;

    private boolean         _verboseMode = false;
    private boolean         _silentMode = false;
    private InputStream     _fis = (InputStream) System.in;
    private OutputStream    _fos = (OutputStream) System.out;
    private String          _cipherFile = DEFAULT_CIPHER_FILENAME;
    private String          _name = System.getProperties().
			    getProperty("user.name");
    private String          _type = AMI_Constants.AMI_USER_OBJECT;
    private String          _hostIP = null;
    private long            _userId;
}
