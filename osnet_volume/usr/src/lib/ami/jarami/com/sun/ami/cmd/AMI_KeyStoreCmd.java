/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_KeyStoreCmd.java	1.1 99/07/11 SMI"
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
import java.lang.Integer;
import sun.misc.BASE64Decoder;
import sun.misc.BASE64Encoder;
import sun.security.pkcs.*;
import sun.security.x509.*;
import sun.security.util.*;

import com.sun.ami.AMI_Exception;
import com.sun.ami.utils.AMI_C_Utils;
import com.sun.ami.utils.AMI_GetOpt;
import com.sun.ami.keymgnt.*;
import com.sun.ami.keygen.*;
import com.sun.ami.common.*;
import com.sun.ami.ca.*;


/**
 * This class implements amisign command.
 *
 * @author Mike Hsu
 *
 * @version 1.0
 *
 */

public class AMI_KeyStoreCmd extends AMI_Common
    implements AMI_KeyMgntService {

    public AMI_KeyStoreCmd() {
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
	AMI_KeyStoreCmd kstore = new AMI_KeyStoreCmd();

	// Set the providers, if not already installed.
	if (Security.getProvider("SunAMI") == null) {
		Security.insertProviderAt(new SunAMI(), 1);
	}

    	initialize(kstore);
	_subcmds = createSubCmdsHashtable();

	try {
	    kstore.parseOption(argv);
	    kstore.run();
	} catch (Exception e) {
	    kstore.prtError(e.getMessage());
	    if (kstore._verboseMode)
	    	e.printStackTrace();
	    kstore.exit(1);
	}
    }

    private static Hashtable createSubCmdsHashtable() {
	
	Hashtable ht = new Hashtable();

	ht.put("genkey", "cvzaklspidehLB");
	ht.put("list", "cvzaphL");
	ht.put("import", "cvtzafphL");
	ht.put("selfcert", "cvzasdiphL");
	ht.put("certreq", "cvzasfphL");
	ht.put("export", "cvtzrafphL");
	ht.put("keypasswd", "cvzonhL");
	ht.put("delete", "cvzaphL");
	ht.put("gencred", "cvzaklspidehLB");
	ht.put("genkeyandcsr", "cvzaklspidehLBf");

	return ht;
    }

    //
    // Parse command line options
    //
    private void parseOption(String[] argv) throws Exception {

        AMI_GetOpt go = new AMI_GetOpt(argv,
	"vhrc:z:a:k:l:s:i:d:p:L:t:f:o:n:e:B:");
	int ch = 01;
	boolean isVirtualHost = false;
	String arg = null;
	StringBuffer optionsOnCmdLine = new StringBuffer("");
	
	/**
	 * Parse and take action for each specified property.
	 */

       	while ((ch = go.getopt()) != go.optEOF) {

		if ((char)ch == 'c') 
		    subcmd = go.optArgGet();
	      	else
	      	if ((char)ch == 'l') {
		    try {
		         arg = go.optArgGet();
			 keysize = new Integer(arg).intValue();
		    } catch (NumberFormatException e) {
		         Object[] args = { new String(arg) };
			 msgFormatter.applyPattern(
		  	 messages.
			 getString("AMI_Cmd.keystore.invalidKeySize"));
			 prtError(msgFormatter.format(args));
			 exit(1);
		    }
	      	}
	      	else
	      	if ((char)ch == 'i') {
		    arg =  go.optArgGet();
		    try {
		        valDays = new Integer(arg).intValue();
			/* 
			* PK: 2/1/99:  Make sure certificate has
			* at least some period of validity 
			*/
		     
			if (valDays < 1) {
			      Object[] args = { new String(arg) };
			      msgFormatter.applyPattern(
			      messages.
			    getString("AMI_Cmd.keystore.invalidValidity"));
			      prtError(msgFormatter.format(args));
			      exit(1);
			}
		    } catch (NumberFormatException e) {
		        Object[] args = { new String(arg) };
			msgFormatter.applyPattern(
			messages.getString("AMI_Cmd.keystore.invalidValidity"));
			prtError(msgFormatter.format(args));
			exit(1);
		    }
	      	} 
	      	else
	      	if ((char)ch == 'a') 
	 	    keyalias = go.optArgGet();
	      	else
	      	if ((char)ch == 'p') 
		    keypass = go.optArgGet();
	      	else
	      	if ((char)ch == 'd') 
	    	    nameDN = go.optArgGet();
	      	else
	      	if ((char)ch == 'e') 
	      	    nameDNS = go.optArgGet();
	      	else
	      	if ((char)ch == 's') 
    		    sigAlgName = go.optArgGet();
	      	else
	      	if ((char)ch == 'k') 
		    keyAlgName = go.optArgGet();
	      	else
	      	if ((char)ch == 'z') 
		    keystorefile = go.optArgGet();
	      	else
	      	if ((char)ch == 'f') 
		    filename = go.optArgGet();
	      	else
	      	if ((char)ch == 'o') 
		    oldpasswd = go.optArgGet();
	      	else
	      	if ((char)ch == 'n') 
		    newpasswd = go.optArgGet();
	      	else
	      	if ((char)ch == 'B') 
		    bulkFile = go.optArgGet();
	      	else
	      	if ((char)ch == 'r') 
		    rfc = true;
	      	else
	      	if ((char)ch == 'v') 
		    _verboseMode = true;
	      	else
	      	if ((char)ch == 't') {
		    arg = go.optArgGet();
	            if (arg.equalsIgnoreCase("keyentry")) {
		        entry_type = KEYENTRY;
		    } else
		    if (arg.equalsIgnoreCase("trustedcert")) {
		        entry_type = TRUSTEDCERT;
		    } else
		    if (arg.equalsIgnoreCase("trustedcacerts")) {
		        entry_type = TRUSTEDCACERTS;
		    } else {
		        Object[] args = { new String(arg) };
			msgFormatter.applyPattern(
			messages.getString("AMI_Cmd.keystore.invalidKeyType"));
			prtError(msgFormatter.format(args));
			Usage();
		    }
	      	} 
	      	else
	      	if ((char)ch == 'h') {
	 	    if (!System.getProperty("user.name").equals("root")) {
		          prtError(messages.
			getString("AMI_Cmd.verify.rootonly"));
		          exit(-1);
		    }
		    amiType = AMI_Constants.AMI_HOST_OBJECT;
		    if (!isVirtualHost)
		          _name = InetAddress.getLocalHost().getHostAddress();
	      	}
	      	else
	      	if ((char)ch == 'L') {
		        String virtual = go.optArgGet();
		        if (virtual.indexOf(".") < 0) {
			    hostIP = InetAddress.getByName(
				virtual).getHostAddress();
		        } else {
			    hostIP = virtual;
		        }
		        AMI_VirtualHost.setHostIP(hostIP);
			_name = hostIP;
			isVirtualHost = true;
	      	} else {
		Usage();
		exit(-1);
	      	}

	      	optionsOnCmdLine.append((char)ch);
	  }

	if (!verifySubCmdOptions(subcmd, optionsOnCmdLine.toString())) {
	    Usage();
	    exit(-1);
	}

	/*
	 * Verify AMI Object type ie., _type
	 */
	if (System.getProperty("user.name").equals("root") &&
	    (amiType != AMI_Constants.AMI_HOST_OBJECT)) {
		prtError(messages.getString("AMI_Cmd.keystore.usehflag"));
		exit(1);
	}

	/*
	 * If -L is specified, then -h has to be specified 
	 */
	if (isVirtualHost && 
	    amiType != AMI_Constants.AMI_HOST_OBJECT) {
	    Usage();
	    exit(1);
	}
	/*
	 * Verify keystorefile, if provided in command line.
	 */
	if (keystorefile != null) {
	    File kf = new File(keystorefile);

	    if (!kf.exists() &&
		!(subcmd.equals("genkey") || subcmd.equals("import"))) {
		    Object[] args = { new String(keystorefile) };
		    msgFormatter.applyPattern(
			messages.getString("AMI_Cmd.fileNotFound"));
		    prtError(msgFormatter.format(args));
		exit(1);
	    }
	}

	/*
	 * Verify sigAlgName and keyAlgName
	 */

	AMI_Debug.debugln(3, "Algorithm for key being used is : " + keyAlgName);
	AMI_Debug.debugln(3, "Signature for key being used is : " + sigAlgName);

    	if (sigAlgName == null)  {
		if (keyAlgName.equalsIgnoreCase("RSA") ||
		keyAlgName.equals("DH")) {
			sigAlgName = "MD5/RSA";
		} else if (keyAlgName.equalsIgnoreCase("DSA")) {
                        sigAlgName = "SHA1/DSA";
		} else {
                    Object[] args = { new String(keyAlgName) };
                    msgFormatter.applyPattern(
                        messages.getString("AMI_Cmd.keystore.noSuchAlgorithm"));
                    prtError(msgFormatter.format(args));
                    exit(1);
		}
	} else if (sigAlgName.equalsIgnoreCase("MD5/RSA") ||
		 sigAlgName.equalsIgnoreCase("MD5withRSA"))
			sigAlgName = "MD5/RSA";
	else if (sigAlgName.equalsIgnoreCase("MD2/RSA") ||
		sigAlgName.equalsIgnoreCase("MD2withRSA")) 
			sigAlgName = "MD2/RSA";
        else if (sigAlgName.equalsIgnoreCase("SHA1/DSA") ||
		sigAlgName.equalsIgnoreCase("SHA/DSA") ||
		sigAlgName.equalsIgnoreCase("SHA1withDSA")) 
			sigAlgName = "SHA1/DSA";
	else {
		    Object[] args = { new String(sigAlgName) };
		    msgFormatter.applyPattern(
			messages.getString("AMI_Cmd.keystore.noSuchAlgorithm"));
		    prtError(msgFormatter.format(args));
		    exit(1);
	}

	/*
	 * Verify keysize
	 */
	if ((keysize % 64) != 0) {
	    Object[] args = { new Integer(keysize).toString() };
	    msgFormatter.applyPattern(
	    messages.getString("AMI_Cmd.keystore.badKeySize"));
	    prtError(msgFormatter.format(args));
	    exit(1);
	}
    }

    private boolean verifySubCmdOptions(String subcmd, 
					String optionsOnCmdLine) 
                            throws Exception
    {        
         String validOptions = (String)_subcmds.get(subcmd);
	 char[] chs = optionsOnCmdLine.toCharArray();

	 for (int ii = 0; ii < chs.length; ii++) {
	 	if (validOptions.indexOf(chs[ii]) == -1) {
	         	return false;
	     	}
	 }
	 return true;	 
    }

    private void Usage() {
	prtError(messages.getString("AMI_Cmd.keystore.usage"));
	 exit(1);
    }

    private void prtMessage(String msg) {
	if (!_silentMode)
	    System.out.println(msg);
    }

    private void prtVerbose(String msg) {
	if (_verboseMode)
	    System.err.println(msg);
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

    private void gencred() throws Exception {
	// For single user gencred is equivalent to doing
	// a genkey, as a self signed cert is added to 
	// key store. For Bulk option, the certificates of
	// each entry in the file, must ne signed by this 
	// user running the command.
        genkey();

	// For single user only.. change this when processing 
	// bulk.

	String owner = new String(System.getProperty("user.name"));
        // Store private key in keystore
        KeyStore keyStore = getKeyStore();
        Certificate[] chain = keyStore.getCertificateChain(keyalias);

	AMI_KeyMgntClient.addX509Certificate(new AMINames(),
	    owner, (X509Certificate)chain[0], AMI_Constants.AMI_USER_OBJECT);
    }

    private void genkeyandcsr() throws Exception {
	genkey();
	certreq();
    }

    private void genkey() throws Exception {

	if (bulkFile != null)
		throw new Exception(
		messages.getString("AMI_Cmd.keystore.invFileFormat"));

	// Prompt for required but unspecified info.
	if (nameDN == null)
	    	nameDN = getNameDN();
	if (keyalias == null)
		keyalias = getKeyalias();
	if (keypass == null)
	    	keypass = getPasswd("");

	if ((keyAlgName.equalsIgnoreCase("RSA")) ||
	    (keyAlgName.equalsIgnoreCase("All")))
		generateRSAkeypair();
	if ((keyAlgName.equalsIgnoreCase("DSA")) ||
	    (keyAlgName.equalsIgnoreCase("All")))
		generateDSAkeypair();
	if ((keyAlgName.equalsIgnoreCase("DH")) ||
	    (keyAlgName.equalsIgnoreCase("All")))
		generateDHkeypair();
    }

    private void generateDSAkeypair() throws Exception {
	// Generate the DSA keys
	String sigAlgName = new String("SHA1/DSA");
	KeyPairGenerator keypairGen = KeyPairGenerator.getInstance("DSA");
	keypairGen.initialize(keysize);

	KeyPair keypair = keypairGen.generateKeyPair();
	PrivateKey privatekey = keypair.getPrivate();

	AMI_X509CertInfo info = new AMI_X509CertInfo();
	info.setVersion(2);
	info.setSerialNumber();
	info.setValidity(365*24*60*60);
	info.setKey(keypair.getPublic());
	info.setSubject(nameDN);
	info.setIssuer(nameDN);
	info.setAlgorithm(sigAlgName);

	AMI_X509CertImpl certificate = new AMI_X509CertImpl(info);
	certificate.sign(privatekey);
	X509Certificate[] chain = new X509Certificate[1];
	chain[0] = certificate;

	// Store private key in keystore
	KeyStore keyStore = getKeyStore();
	keyStore.setKeyEntry(keyalias, privatekey,
	    keypass.toCharArray(), chain);

	putKeyStore(keyStore);

	// Construct the default DSA usage alias if necessary
	if (getDSAAlias(amiType) == null) {
		Object[] args = { new String(keyalias) };
		msgFormatter.applyPattern(
		    messages.getString("AMI_Cmd.keystore.defRSAKey"));
		prtMessage(msgFormatter.format(args));
		setDSAAlias(keyalias, amiType);
	}
    }

    private void generateRSAkeypair()
	throws Exception {
	String answer = null;

	// Generate the RSA keys
	KeyPairGenerator keypairGen = KeyPairGenerator.getInstance("RSA");

	keypairGen.initialize(keysize);

	KeyPair keypair = keypairGen.generateKeyPair();
	PrivateKey rsaPrivatekey = keypair.getPrivate();

	// Generate self signed certificate
	AMI_X509CertInfo info = new AMI_X509CertInfo();
	info.setVersion(2);
	info.setSerialNumber();
	info.setValidity(valDays*24*60*60);
	info.setKey(keypair.getPublic());
	info.setSubject(nameDN.toUpperCase());
	info.setIssuer(nameDN.toUpperCase());
	info.setAlgorithm(sigAlgName);

	AMI_X509CertImpl certificate = new AMI_X509CertImpl(info);
	certificate.sign(rsaPrivatekey);
	X509Certificate[] chain = new X509Certificate[1];
	chain[0] = certificate;

	// Get keystore instance
	KeyStore keyStore = getKeyStore();
	keyStore.setKeyEntry(keyalias, rsaPrivatekey,
	    keypass.toCharArray(), chain);

	// Store keystore
	putKeyStore(keyStore);

	// Construct the default RSA usage alias if necessary	
	prtMessage(messages.getString("AMI_Cmd.keystore.RSAKeyGen"));
	if (getRSASignAlias(amiType) == null) {
	    Object[] args = { new String(keyalias) };
	    msgFormatter.applyPattern(
		messages.getString("AMI_Cmd.keystore.defSigKey"));
	    prtMessage(msgFormatter.format(args));
	    setRSASignAlias(keyalias, amiType);
	}
	if (getRSAEncryptAlias(amiType) == null) {
	    Object[] args = { new String(keyalias) };
	    msgFormatter.applyPattern(
		messages.getString("AMI_Cmd.keystore.defCryptKey"));
	    prtMessage(msgFormatter.format(args));
	    setRSAEncryptAlias(keyalias, amiType);
	}

    }

    private void generateDHkeypair() throws Exception {
	// Generate the DH keys
	String sigAlgName = new String("MD5/RSA");
	KeyPairGenerator keypairGen = null;
	try {
	    keypairGen = KeyPairGenerator.getInstance("DH");
	} catch (NoSuchAlgorithmException e) {
		Object[] args = { new String("DH") };
		msgFormatter.applyPattern(
			messages.getString("AMI_Cmd.keystore.noSuchAlgorithm"));
		prtError(msgFormatter.format(args));
		exit(1);
	}

	keypairGen.initialize(keysize);

	KeyPair keypair = keypairGen.generateKeyPair();
	PrivateKey privatekey = keypair.getPrivate();
	PrivateKey rsaPrivatekey = getRSASignPrivateKey();

	AMI_X509CertInfo info = new AMI_X509CertInfo();
	info.setVersion(2);
	info.setSerialNumber();
               info.setValidity(365*24*60*60);
	info.setKey(keypair.getPublic());
	info.setSubject(nameDN);
	info.setIssuer(nameDN);
	info.setAlgorithm(sigAlgName);

	AMI_X509CertImpl certificate = new AMI_X509CertImpl(info);
	certificate.sign(rsaPrivatekey);
	X509Certificate[] chain = new X509Certificate[1];
	chain[0] = certificate;

	// Store private key in keystore
	KeyStore keyStore = null;
	if ((keyStore = getKeyStore()) == null)
	  	keyStore = KeyStore.getInstance("jks");
	keyStore.setKeyEntry(keyalias, privatekey,
	    keypass.toCharArray(), chain);

	putKeyStore(keyStore);

	prtMessage(messages.getString("AMI_Cmd.keystore.DHKeyGen"));
	// Construct the default DH usage alias if necessary
	if (getDHAlias(amiType) == null) {
	    Object[] args = { new String(keyalias) };
	    msgFormatter.applyPattern(
		messages.getString("AMI_Cmd.keystore.defDHKey"));
	    prtMessage(msgFormatter.format(args));
	    setDHAlias(keyalias, amiType);
	}
    }
    // Import from 
	/**
	 * Method to get the default RSA Signature key
	 */
	public PrivateKey getRSASignPrivateKey() throws Exception {
		return (getRSASignPrivateKey(AMI_Constants.AMI_USER_OBJECT));
	}

	/**
	 * Method to get the default RSA Signature key, given ami type
	 */
	public PrivateKey getRSASignPrivateKey(String amiType)
	    throws Exception {
		// Get the default RSA signature key alias
		String rsaSignAlias = getRSASignAlias(amiType);

		KeyStore keyStore = getKeyStore();
		if (keyStore == null)
			return (null);
		return ((PrivateKey) (keyStore.getKey(rsaSignAlias,
		    keypass.toCharArray())));
	}

	/**
	 * Method to get the default RSA sign key alias
	 */
	public String getRSASignAlias() throws Exception {
		return (getRSASignAlias(AMI_Constants.AMI_USER_OBJECT));
	}

	/**
	 * Method to get the default RSA sign key alias, given AMI type
	 */
	public String getRSASignAlias(String amiType) throws Exception {
		// Get the default RSA signature key alias
		return (AMI_KeyMgntClient.getRSASignAlias(_name, amiType));
	}

	/**
	 * Method to set the default RSA sign key alias
	 */
	public void setRSASignAlias(String alias) throws Exception {
		setRSASignAlias(alias,
		    AMI_Constants.AMI_USER_OBJECT);
	}

	/**
	 * Method to set the default RSA sign key alias, given AMI type
	 */
	public void setRSASignAlias(String alias, String amiType)
	    throws Exception {
		// Set the default RSA signature key alias
		AMI_KeyMgntClient.setRSASignAlias(this, _name,
		    alias, amiType);
	}

	/**
	 * Method to get the default RSA Signature certificate
	 */
	public Certificate getRSASignCertificate() throws Exception {
		return (getRSASignCertificate(AMI_Constants.AMI_USER_OBJECT));
	}

	/**
	 * Method to get the default RSA Signature certificate, given AMI type
	 */
	public Certificate getRSASignCertificate(String amiType)
	    throws Exception {
		// Get the default RSA signature key alias
		String rsaSignAlias = getRSASignAlias();

		KeyStore keyStore = getKeyStore();
		if (keyStore == null)
			return (null);
		return (keyStore.getCertificateChain(rsaSignAlias)[0]);
	}

	/**
	 * Method to get the default RSA Encryption key alias
	 */
	public String getRSAEncryptAlias() throws Exception {
		return (getRSAEncryptAlias(AMI_Constants.AMI_USER_OBJECT));
	}

	/**
	 * Method to get the default RSA Encryption key alias
	 */
	public String getRSAEncryptAlias(String amiType) throws Exception {
		// Get the default RSA encryption key alias
		return (AMI_KeyMgntClient.getRSAEncryptAlias(_name,
		    amiType));
	}

	/**
	 * Method to set the default RSA Encryption key alias
	 */
	public void setRSAEncryptAlias(String alias) throws Exception {
		setRSAEncryptAlias(alias, AMI_Constants.AMI_USER_OBJECT);
	}

	/**
	 * Method to set the default RSA Encryption key alias
	 */
	public void setRSAEncryptAlias(String alias, String amiType)
	    throws Exception {
		// Set the default RSA encryption key alias
		AMI_KeyMgntClient.setRSAEncryptAlias(this, _name,
		    alias, amiType);
	}

	/**
	 * Method to get the default RSA Encryption key
	 */
	public PrivateKey getRSAEncryptPrivateKey() throws Exception {
		return (getRSAEncryptPrivateKey(AMI_Constants.AMI_USER_OBJECT));
	}

	/**
	 * Method to get the default RSA Encryption key
	 */
	public PrivateKey getRSAEncryptPrivateKey(String amiType)
	    throws Exception {
		// Get the default RSA encryption key alias
		String rsaEncryptAlias = getRSAEncryptAlias();

		KeyStore keyStore = getKeyStore();
		if (keyStore == null)
			return (null);
		return ((PrivateKey) (keyStore.getKey(rsaEncryptAlias,
		    keypass.toCharArray())));
	}

	/**
	 * Method to get the default RSA Encryption Certificate
	 */
	public Certificate getRSAEncryptCertificate() throws Exception {
		return (getRSAEncryptCertificate(
		    AMI_Constants.AMI_USER_OBJECT));
	}

	/**
	 * Method to get the default RSA Encryption Certificate
	 */
	public Certificate getRSAEncryptCertificate(String amiType)
	    throws Exception {
		// Get the default RSA encryption key alias
		String rsaEncryptAlias = getRSAEncryptAlias();

		KeyStore keyStore = getKeyStore();
		if (keyStore == null)
			return (null);
		return (keyStore.getCertificateChain(rsaEncryptAlias)[0]);
	}

	/**
	 * Method to get the default DSA key alias
	 */
	public String getDSAAlias() throws Exception {
		return (getDSAAlias(AMI_Constants.AMI_USER_OBJECT));
	}

	/**
	 * Method to get the default DSA key alias
	 */
	public String getDSAAlias(String amiType) throws Exception {
		return (AMI_KeyMgntClient.getDSAAlias(_name, amiType));
	}

	/**
	 * Method to set the default DSA key alias
	 */
	public void setDSAAlias(String alias) throws Exception {
		setDSAAlias(alias, AMI_Constants.AMI_USER_OBJECT);
	}

	/**
	 * Method to set the default DSA key alias
	 */
	public void setDSAAlias(String alias, String amiType)
	    throws Exception {
		AMI_KeyMgntClient.setDSAAlias(this,
		    _name, alias, amiType);
	}

	/**
	 * Method to get the default DSA key
	 */
	public PrivateKey getDSAPrivateKey() throws Exception {
		return (getDSAPrivateKey(AMI_Constants.AMI_USER_OBJECT));
	}

	/**
	 * Method to get the default DSA private key
	 */
	public PrivateKey getDSAPrivateKey(String amiType) throws Exception {
		// Get the default DSA key alias
		String dsaSignAlias = getDSAAlias(amiType);

		KeyStore keyStore = getKeyStore();
		if (keyStore == null)
			return (null);
		return ((PrivateKey) (keyStore.getKey(dsaSignAlias,
		    keypass.toCharArray())));
	}

	/**
	 * Method to get the default DSA Certificate
	 */
	public Certificate getDSACertificate() throws Exception {
		return (getDSACertificate(AMI_Constants.AMI_USER_OBJECT));
	}

	/**
	 * Method to get the default DSA Certificate
	 */
	public Certificate getDSACertificate(String amiType) throws Exception {
		// Get the default DSA key alias
		String dsaSignAlias = getDSAAlias(amiType);

		KeyStore keyStore = getKeyStore();
		if (keyStore == null)
			return (null);
		return (keyStore.getCertificateChain(dsaSignAlias)[0]);
	}

	/**
	 * Method to get the default DH key Alias
	 */
	public String getDHAlias() throws Exception {
		return (getDHAlias(AMI_Constants.AMI_USER_OBJECT));
	}

	/**
	 * Method to get the default DH key Alias
	 */
	public String getDHAlias(String amiType) throws Exception {
		return (AMI_KeyMgntClient.getDHAlias(_name, amiType));
	}

	/**
	 * Method to set the default DH key Alias
	 */
	public void setDHAlias(String alias) throws Exception {
		setDHAlias(alias, AMI_Constants.AMI_USER_OBJECT);
	}

	/**
	 * Method to set the default DH key Alias
	 */
	public void setDHAlias(String alias, String amiType)
	    throws Exception {
		AMI_KeyMgntClient.setDHAlias(this, _name,
		    alias, amiType);
	}

	/**
	 * Method to get the default DH private key
	 */
	public PrivateKey getDHPrivateKey() throws Exception {
		return (getDHPrivateKey(AMI_Constants.AMI_USER_OBJECT));
	}

	/**
	 * Method to get the default DH private key
	 */
	public PrivateKey getDHPrivateKey(String amiType) throws Exception {
		// Get the default DH key alias
		String dhSignAlias = getDHAlias(amiType);

		KeyStore keyStore = getKeyStore();
		if (keyStore == null)
			return (null);
		return ((PrivateKey) (keyStore.getKey(dhSignAlias,
		    keypass.toCharArray())));
	}

	/**
	 * Method to get the default DH Certificate
	 */
	public Certificate getDHCertificate() throws Exception {
		return (getDHCertificate(AMI_Constants.AMI_USER_OBJECT));
	}

	/*
	 * Method to get the default DH Certificate
	 */
	private Certificate getDHCertificate(String amiType) throws Exception {
		// Get the default DH key alias
		String dhSignAlias = getDHAlias(amiType);

		KeyStore keyStore = getKeyStore();
		if (keyStore == null)
			return (null);
		return (keyStore.getCertificateChain(dhSignAlias)[0]);
	}

	/*
	 * Method to get loaded KeyStore
	 */
	private KeyStore getKeyStore() {
		KeyStore keyStore = null;

		try {
			// If the keystore file is specified
			//  in the command line, use it.
			if (keystorefile != null) {
				File kf = new File(keystorefile);
				keyStore = KeyStore.getInstance("jks");

				if (!kf.exists() || !kf.canRead()) {
					keyStore.load(null, null);
				} else {
					keyStore.load(new FileInputStream(kf),
				    	    keypass.toCharArray());
				}
				return (keyStore);
			}

			// Get the KeyStore from AMI_KeyMgntClient
			keyStore = AMI_KeyMgntClient.getKeyStore(_name,
			    "keystoreRSA", keypass, amiType);
			if (keyStore == null)
				keyStore = KeyStore.getInstance("jks");
			keyStore.load(null, null);
		} catch (Exception e) {
			prtError(
			    messages.getString("AMI_Cmd.keystore.noKeyStore"));
			prtVerbose(e.getMessage());
			exit(1);
		}
		return (keyStore);
	}

	/*
	 * Method to put KeyStore
	 */
	private void putKeyStore(KeyStore keyStore) throws Exception {

		// If keystore file is provided in command line, use it
		if (keystorefile != null) {
			File kf = new File(keystorefile);
			keyStore.store(new FileOutputStream(kf),
			    keypass.toCharArray());
		} else {
		    // Store KeyStore using AMI_KeyMgntClient
		    try {
	    	    	AMI_KeyMgntClient.setKeyStore(this, _name,
	    	            "keystoreRSA", keyStore, keypass, amiType);
		   } catch (Exception e) {
			e.printStackTrace();
		   }
		}

		if ((keyStore == null) || (keyStore.size() == 0)) {
			// Just the delete the keystore
			AMI_KeyMgntClient.deleteKeyStore(_name,
			    "keystoreRSA", amiType);
			return;
		}

	}

	/**
	 * Method to put Certificate, given indexName
	 */
	public void putX509Certificate(String userName, X509Certificate cert)
	    throws Exception {
		putX509Certificate(userName, cert,
		    AMI_Constants.AMI_USER_OBJECT);
	}

	/**
	 * Method to put Certificate, given indexName
	 */
	public void putX509Certificate(String userName, X509Certificate cert,
	    String amiType) throws Exception {
		AMI_KeyMgntClient.addX509Certificate(this,
		    userName, cert, amiType);
	}

	/**
	 * Method to put Certificates
	 */
	public void putX509Certificate(X509Certificate cert)
	    throws Exception {
		putX509Certificate(_name, cert);
	}

	/**
	 * Method to put Certificates
	 */
	public void putX509Certificate(X509Certificate cert,
	    String amiType) throws Exception {
		putX509Certificate(_name, cert, amiType);
	}

	/**
	 * Method to put get all X509 Certificates
	 */
	public Enumeration getAllX509Certificates() throws Exception {
		return (getAllX509Certificates(
		    AMI_Constants.AMI_USER_OBJECT));
	}

	/**
	 * Method to put get all X509 Certificates
	 */
	public Enumeration getAllX509Certificates(String amiType)
	    throws Exception {
		return (AMI_KeyMgntClient.getX509Certificates(_name,
		     amiType));
	}

	/**
	 * Method to delete Certificates
	 */
	public void deleteX509Certificate(X509Certificate cert)
	    throws Exception {
		deleteX509Certificate(cert, AMI_Constants.AMI_USER_OBJECT);
	}

	/**
	 * Method to delete Certificates
	 */
	public void deleteX509Certificate(X509Certificate cert, String amiType)
	    throws Exception {
		AMI_KeyMgntClient.deleteX509Certificate(_name, cert,
		    amiType);
	}

    private AMI_CertReq generateCertReq(KeyPair keypair) 
	throws Exception {
	     
	PrivateKey privateKey;

	AMI_CertReqInfo info = new AMI_CertReqInfo();

	info.setPublicKey(keypair.getPublic());

	// Get subject nameDN
	if (nameDN == null)
	    nameDN = getNameDN();

	try {
                info.setSubject(nameDN);
	} catch (IOException e) {
                prtMessage(e.getMessage());
		throw e;
	}

	AMI_CertReq certreq = new AMI_CertReq(info);

	// Set signature algo to DSA if DSA keys, otherwise default
	// is RSA signature.
	if (keypair.getPrivate().getAlgorithm().endsWith("DSA")) {
	    if (!sigAlgName.equals("SHA1/DSA")) {
		prtVerbose("Unapplicable algorithm: " + sigAlgName +
			". Use SHA1/DSA.");
		sigAlgName = "SHA1/DSA";
	    }
	    try {
	    	privateKey = getDSAPrivateKey();
	    } catch (Exception e) {
                prtMessage(e.getMessage());
		throw e;
	    }
	} else {
	    if (!sigAlgName.equals("MD5/RSA") &&
		!sigAlgName.equals("MD2/RSA")) {
		prtVerbose("Unapplicable algorithm: " + sigAlgName +
			". Use MD5/RSA.");
		sigAlgName = "MD5/RSA";
	    }
	    try {
	    	privateKey = getRSASignPrivateKey();
	    } catch (Exception e) {
                prtMessage(e.getMessage());
		throw e;
	    }
	}
	          
	AMI_Debug.debugln(3, "generateCertReq:: Using algo " + sigAlgName + 
			  " for sign on certreq");

	try {
                certreq.encodeAndSign(privateKey, sigAlgName);
	} catch (Exception e) {
                prtMessage(e.getMessage());
		throw e;
	}

	return certreq;
    }
    // End of import

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

	// Since JCE will not be present in the export version
	// of AMI, we should NOT add this.
	//
	// if (Security.getProvider("SunJCE") == null) {
	//    Security.insertProviderAt(new SunJCE(), 2);
	// }

	if (subcmd.equals("genkey")) {
	    genkey();
	} else
	if (subcmd.equals("gencred")) {
	    gencred();
	} else
	if (subcmd.equals("list")) {
	    list();
	} else
	if (subcmd.equals("genkeyandcsr")) {
	    genkeyandcsr();
	} else
	if (subcmd.equals("delete")) {
	    delete();
	} else
	if (subcmd.equals("certreq")) {
	    certreq();
	} else
	if (subcmd.equals("selfcert")) {
	    selfcert();
	} else
	if (subcmd.equals("keypasswd")) {
	    keypasswd();
	} else
	if (subcmd.equals("import")) {
	    do_import();
	} else
	if (subcmd.equals("export")) {
	    do_export();
	} else
	    Usage();

	exit(0);
    }

    private void list() throws Exception {
	// Get keystore instance
	KeyStore keyStore = null;
	BASE64Encoder encoder = new BASE64Encoder();
	/*
	 * Prompt for missing info.
	 */
	if (keypass == null)
	    keypass = getPasswd("");

	if ((keyStore = getKeyStore()) == null) {
	  	keyStore = KeyStore.getInstance("jks");
	}

	// Enumeration
	Enumeration e = keyStore.aliases();

	while (e.hasMoreElements()) {
	    String alias = (String) e.nextElement();
	    Key key;
	    Certificate cert;

	    // If an alias is specified, only display that alias.
	    if ((keyalias != null) && !keyalias.equalsIgnoreCase(alias))
		continue;

	    if (keyStore.isCertificateEntry(alias)) {
		prtMessage(messages.getString("AMI_Cmd.keystore.TCRHeader"));
		prtMessage(alias);
		prtMessage(messages.getString("AMI_Cmd.CommaSpace"));
		prtMessage(keyStore.getCreationDate(alias).toString());
	        cert = keyStore.getCertificate(alias);
		prtMessage(cert.toString());
		prtMessage(messages.getString("AMI_Cmd.newline"));
	    }
	    if (keyStore.isKeyEntry(alias)) {
		prtMessage(messages.getString("AMI_Cmd.keystore.KeyEntHeader"));
		prtMessage(alias);
		prtMessage(messages.getString("AMI_Cmd.CommaSpace"));
		prtMessage(keyStore.getCreationDate(alias).toString());
	        key = keyStore.getKey(alias, keypass.toCharArray());
		prtMessage(messages.getString("AMI_Cmd.keystore.KeyAlgo"));
		prtMessage(key.getAlgorithm());
		prtMessage(messages.getString("AMI_Cmd.newline"));

	      	prtMessage(messages.getString("AMI_Cmd.keystore.certChainHdr"));
		prtMessage(alias);
		prtMessage(messages.getString("AMI_Cmd.CommaSpace"));
		prtMessage(keyStore.getCreationDate(alias).toString());

		Certificate[] certChain = keyStore.getCertificateChain(alias);
		
		for (int ii = 0; ii < certChain.length; ii++) {
	    	    Object[] args = { new String(new Integer(
			ii + 1).toString())};
	    	    msgFormatter.applyPattern(
			messages.getString("AMI_Cmd.keystore.certNoInChain"));
	    	    prtMessage(msgFormatter.format(args));
		    prtMessage(certChain[ii].toString());
		}
		prtMessage(messages.getString("AMI_Cmd.newline"));
	    }
	}
    }

    private void delete() throws Exception {
	// Get keystore instance
	KeyStore keyStore = null;

	/*
	 * Prompt for missing info.
	 */
	if (keyalias == null)
		keyalias = getKeyalias();
	if (keypass == null)
	    keypass = getPasswd("");

	// get keystore
	if ((keyStore = getKeyStore()) == null) {
	  	keyStore = KeyStore.getInstance("jks");
	}

	// Enumeration
	Enumeration e = keyStore.aliases();


	{
	    Object[] args = { new String(keyalias) };
	    msgFormatter.applyPattern(
		messages.getString("AMI_Cmd.keystore.deleteKey"));
	    prtMessage(msgFormatter.format(args));
	}

	while (e.hasMoreElements()) {
	    String alias = (String) e.nextElement();
//	    prtMessage(alias);
	    if (keyalias.equals(alias)) {
	        Object[] args = { new String(alias) };
	        msgFormatter.applyPattern(
		    messages.getString("AMI_Cmd.keystore.deleteAlias"));
	        prtMessage(msgFormatter.format(args));
		keyStore.deleteEntry(alias);
	    }
	}
	putKeyStore(keyStore);
    }

    private void certreq() throws Exception {
	// Get keystore instance
	KeyStore keyStore = null;
	Certificate chain[];
        PrivateKey privatekey = null;
        PublicKey publickey = null;
	AMI_CertReq certreq = null;

	/*
	 * Prompt for required (but unspecified) info.
	 */
	if (keyalias == null) {
		keyalias = AMI_KeyMgntClient.getRSASignAlias(_name, amiType);
		if (keyalias == null)
			keyalias = getKeyalias();
	}
	if (keypass == null) {
	    keypass = getPasswd("");
	}
	if ((keyStore = getKeyStore()) == null) {
	  	keyStore = KeyStore.getInstance("jks");
	}

	if (!keyStore.isKeyEntry(keyalias)) {
	    Object[] args = { new String(keyalias) };
	    msgFormatter.applyPattern(
		messages.getString("AMI_Cmd.keystore.notKeyEntry"));
	    prtError(msgFormatter.format(args));
	    exit(1);
	}
	// Redirect output to file, if specified.
	if (filename != null)
		_output = new FileOutputStream(filename);

	// Make sure we can access the private key!
	privatekey = (PrivateKey)(keyStore.getKey(keyalias,
		keypass.toCharArray()));
	chain = keyStore.getCertificateChain(keyalias);
	publickey = chain[0].getPublicKey();

	AMI_Debug.debugln(3, "certreq:: Generating certreq for " + keyalias);

	certreq = generateCertReq(new KeyPair(publickey, privatekey));
	certreq.print(new PrintStream(_output));
    }

    private void selfcert() throws Exception {
	KeyStore keyStore = null;
	Certificate chain[];
        PrivateKey privatekey = null;
        PublicKey publickey = null;
	/*
	 * Prompt for required (but unspecified) info.
	 */
	if (nameDN == null)
	    	nameDN = getNameDN();
	if (keyalias == null)
		keyalias = getKeyalias();
	if (keypass == null)
	    keypass = getPasswd("");

	// Get keystore instance
	if ((keyStore = getKeyStore()) == null) {
	  	keyStore = KeyStore.getInstance("jks");
	}


	if (!keyStore.isKeyEntry(keyalias)) {
	    Object[] args = { new String(keyalias) };
	    msgFormatter.applyPattern(
		messages.getString("AMI_Cmd.keystore.notKeyEntry"));
	    prtError(msgFormatter.format(args));
	    exit(1);
	}
	privatekey = (PrivateKey)(keyStore.getKey(keyalias,
		keypass.toCharArray()));
	chain = keyStore.getCertificateChain(keyalias);
	publickey = chain[0].getPublicKey();

	// Generate self signed certificate
	AMI_X509CertInfo info = new AMI_X509CertInfo();
	info.setVersion(2);
	info.setSerialNumber();
	info.setValidity(valDays*24*60*60);
	info.setKey(publickey);
	info.setSubject(nameDN.toUpperCase());
	info.setIssuer(nameDN.toUpperCase());
	info.setAlgorithm(sigAlgName);

	AMI_X509CertImpl certificate = new AMI_X509CertImpl(info);
	certificate.sign(privatekey);
	X509Certificate[] xchain = new X509Certificate[1];
	xchain[0] = certificate;

	keyStore.setKeyEntry(keyalias, privatekey,
		    keypass.toCharArray(), xchain);

	// Store keystore
	putKeyStore(keyStore);

	// Store Certificate
	// putX509Certificate(xchain[0], amiType);
    }

    private void keypasswd() throws Exception {
	KeyStore okeyStore, nkeyStore;
        String alias;
	/*
	 * Prompt for required (but unspecified) info.
	 */
	if (oldpasswd == null)
		oldpasswd = getPasswd("Enter OLD passwd: ");
	if (newpasswd == null)
		newpasswd = getPasswd("Enter NEW passwd: ");

	// Check if unchanged
	if (newpasswd.equals(oldpasswd)) {
	    prtError(messages.getString("AMI_Cmd.keystore.passwdUnchanged"));
	    exit(1);
	}

	//
	keypass = oldpasswd;

	okeyStore = getKeyStore();
        nkeyStore = KeyStore.getInstance("jks");
        nkeyStore.load(null, null);

        Enumeration enum = okeyStore.aliases();
                
	while (enum.hasMoreElements()) {
	    alias = (String) enum.nextElement();
            if (okeyStore.isKeyEntry(alias)) {
		nkeyStore.setKeyEntry(alias, (PrivateKey)
			(okeyStore.getKey(alias, oldpasswd.toCharArray())),
			newpasswd.toCharArray(), 
			okeyStore.getCertificateChain(alias));
            } else {
            	nkeyStore.setCertificateEntry(alias,
                    okeyStore.getCertificate(alias));
            }
        }
	// Set keypass to newpasswd
	keypass = newpasswd;
	putKeyStore(nkeyStore);
    }

    private void do_import() throws Exception {
	if (keypass == null)
	    keypass = getPasswd("");

	// Get local keystore
	KeyStore keyStore = null;
	if ((keyStore = getKeyStore()) == null) {
	    keyStore = KeyStore.getInstance("jks");
	}

	/*
	 * Redirect input
	 */
	if (filename != null) {
	    File kf = new File(filename);
	    _input = new FileInputStream(kf);
	}

	if (entry_type == KEYENTRY) {
	    // Load the import keystore
	    KeyStore nkeyStore = KeyStore.getInstance("jks");
	    nkeyStore.load(_input, keypass.toCharArray());

	    // Get key aliases
	    Enumeration e = nkeyStore.aliases();
    
	    while (e.hasMoreElements()) {
	    	String alias = (String) e.nextElement();

	        // If an alias is specified, only display that alias.
	        if ((keyalias != null) && !keyalias.equalsIgnoreCase(alias))
		    continue;

		prtVerbose("adding key " + alias + " to keystore.");

		keyStore.setKeyEntry(alias, (PrivateKey)
			    (nkeyStore.getKey(alias, keypass.toCharArray())),
			    keypass.toCharArray(), 
			    nkeyStore.getCertificateChain(alias));
	    }
	    putKeyStore(keyStore);
	} else
	if (entry_type == TRUSTEDCACERTS) {
	    PrivateKey privkey = null;

	    if (keyalias == null)
		getKeyalias();

	    try {
	        privkey = (PrivateKey)keyStore.getKey(keyalias,
					keypass.toCharArray());
	    } catch (Exception e) {
	        Object[] args = { new String(keyalias) };
	        msgFormatter.applyPattern(
		    messages.getString("AMI_Cmd.keystore.keyNotExist"));
	        prtError(msgFormatter.format(args));
		exit(1);
	    }

    	    Certificate[] certchain =
		getCertificateChainFromInputStream(_input);

	    keyStore.setKeyEntry(keyalias, privkey,
		    keypass.toCharArray(), certchain);
	    putKeyStore(keyStore);
	} else {
	    // TRUSTEDCERT
    	    Certificate[] certchain =
		getCertificateChainFromInputStream(_input);

	    if (keyalias == null)
		getKeyalias();

	    keyStore.setCertificateEntry(keyalias, certchain[0]);

	    putKeyStore(keyStore);
	}
    }

    private Certificate[] getCertificateChainFromInputStream(InputStream in)
	throws Exception {
	/*
	 * read input file to a string
	 */
	String dataString = null;
	int cnt = _input.available();
	byte[] dataByte = new byte[cnt];
	in.read(dataByte);
	dataString = new String(dataByte);

	System.out.println("data = " + dataString);
	if (!dataString.startsWith(beginCertificate) ||
	    !dataString.endsWith(endCertificate))
		throw new AMI_Exception("Invalid Certificate File");

	int certBeginIndex = beginCertificate.length();
	int certEndIndex = dataString.indexOf(endCertificate);
	String encodedCert;
	Certificate cert;
	Vector certificates = new Vector();
	while (true) {
		encodedCert = dataString.substring(certBeginIndex,
		certEndIndex);
		cert = getCertificateFromEncodedString(encodedCert);
		certificates.add(cert);
		// Pointer to next element
		if ((certBeginIndex = dataString.indexOf(
		    beginCertificate, certBeginIndex)) == -1)
			break;
		certBeginIndex += beginCertificate.length();
		if ((certEndIndex = dataString.indexOf(
		    endCertificate, certEndIndex)) == -1)
			break;
	}
	Certificate certchain[] = new Certificate[certificates.size()];
	Enumeration enum = certificates.elements();
	int i = 0;
	while (enum.hasMoreElements())
		certchain[i++] = (Certificate) enum.nextElement();
	return (certchain);
    }

    private Certificate getCertificateFromEncodedString(String encodedCert)
	throws Exception {
	BASE64Decoder base64 = new BASE64Decoder();
	byte cert_bytes[] = base64.decodeBuffer(encodedCert);

	CertificateFactory cf = CertificateFactory.getInstance(
						"X509", "SunAMI");

	Certificate certificate = cf.generateCertificate(
				     new ByteArrayInputStream(cert_bytes));
	return (certificate);
    }

    private void do_export() throws Exception {
	int nfound = 0;
	KeyStore keyStore = null;

	// Prepare an empty keystore t oexport keyentry
	KeyStore nkeyStore = KeyStore.getInstance("jks");
	nkeyStore.load(null, null);
	/*
	 * Prompt for required info
	 */
	// Must specified keyalias if exporting trusted cert
	if ((keyalias == null) && (entry_type != KEYENTRY))
	    keyalias = getKeyalias();

	if (keypass == null)
	    keypass = getPasswd("");

	// Get keystore instance
	if ((keyStore = getKeyStore()) == null) {
	  	keyStore = KeyStore.getInstance("jks");
	}
	/*
	 * Redirect output stream
	 */
	if (filename != null) {
	    File kf = new File(filename);
	    _output = new FileOutputStream(kf);
	}

	// Get key aliases
	Enumeration e = keyStore.aliases();

	while (e.hasMoreElements()) {
	    String alias = (String) e.nextElement();

	    // If an alias is specified, only display that alias.
	    if ((keyalias != null) && !keyalias.equalsIgnoreCase(alias))
		continue;

	    nfound++;

	    if ((entry_type == KEYENTRY) || (entry_type == TRUSTEDCACERTS)) {
	    	if (keyStore.isKeyEntry(alias)) {
		    if (entry_type == KEYENTRY) {
			nkeyStore.setKeyEntry(alias, (PrivateKey)
			    (keyStore.getKey(alias, keypass.toCharArray())),
			    keypass.toCharArray(), 
			    keyStore.getCertificateChain(alias));
		    } else {
			X509Certificate cert;
			Certificate chain[] =
			    keyStore.getCertificateChain(alias);
			// Export Only certificate chain
			for (int i = 0; i < chain.length; i++) {
				cert = (X509Certificate) chain[i];
				exportX509Certificate(cert, _output);
			}
		    }
		}
	    } else {
	    	if (keyStore.isCertificateEntry(alias)) {
		    Certificate cert = keyStore.getCertificate(alias);
		    exportX509Certificate((X509Certificate)cert, _output);
		} else
		if (keyalias != null) {
	    	    Object[] args = { new String(alias) };
	    	    msgFormatter.applyPattern(
			messages.getString("AMI_Cmd.keystore.notTrustedCert"));
	    	    prtError(msgFormatter.format(args));
		    exit(1);
		}
	    }
	}

	if (nfound == 0) {
	    Object[] args = { new String(keyalias) };
	    msgFormatter.applyPattern(
		messages.getString("AMI_Cmd.keystore.notKeyEntry"));
	    prtError(msgFormatter.format(args));
	    exit(1);
	}

	if (entry_type == KEYENTRY) {
	    nkeyStore.store(_output, keypass.toCharArray());
	}
	/*
	 * Certificates have already been output
	 */
    }

	private void exportX509Certificate(X509Certificate cert,
	    OutputStream out) {
		BASE64Encoder encoder = new BASE64Encoder();
		try {
		    if (rfc) {
			out.write(new String(
			    "-----BEGIN CERTIFICATE-----\n").getBytes());
			encoder.encodeBuffer(cert.getEncoded(), out);
			out.write(new String(
			    "-----END CERTIFICATE-----\n").getBytes());
		    } else {
			out.write(cert.getEncoded());
		    }
		} catch (CertificateEncodingException c) {
			c.printStackTrace();
		} catch (IOException io) {
			io.printStackTrace();
		}
	}


    /**
     * Initialize internationalization.
     */
    private static void initialize(AMI_KeyStoreCmd kstore) {

	try {
       	    Locale currentLocale = AMI_Constants.initLocale();
       	    msgFormatter = new MessageFormat("");
       	    msgFormatter.setLocale(currentLocale);
       	    messages = AMI_Constants.getMessageBundle(
		currentLocale);
     	} catch (Exception e) {
	    kstore.prtMessage(e.getMessage());
	    e.printStackTrace();
     	}
    }

    private String getKeyalias() throws IOException {
	if (keyalias == null) {
	    BufferedReader reader = new BufferedReader(
					new InputStreamReader(System.in));
	    System.out.print("Enter keyalias: ");
	    keyalias = reader.readLine();
	}
	return keyalias;
    }

    private String getPasswd(String prompt) {
	String passwd = null;
	/*
	    BufferedReader reader = new BufferedReader(
					new InputStreamReader(System.in));
	    System.out.print("Enter passwd: ");
	    keypass = reader.readLine();
	*/
	try {
	    passwd = AMI_C_Utils.ami_get_password(prompt,
	   			AMI_Constants.AMI_USER_LOGIN);
	} catch (AMI_Exception e) {
	    prtError(messages.getString("AMI_Cmd.keystore.noPasswd"));
	    exit(1);
	}

	return passwd;
    }

    public String getNameLogin() {
	 return _name;
    }

    public String getNameDN() {
	String answer, dname, orgu, org, st, co;
	if (nameDN != null)
		return (nameDN);

        // Get current users dn name 
        try {
		nameDN = AMI_KeyMgntClient.getDNNameFromLoginName(
		    _name, amiType);
        } catch (Exception e) {
		nameDN = null;
        }

	BufferedReader reader = new BufferedReader(
	    new InputStreamReader(System.in));

	dname = orgu = org = st = co = "unknown";

	do {
	  try {
	    if (nameDN == null) {
		if (amiType == AMI_Constants.AMI_USER_OBJECT) {
			prtMessage(messages.getString(
			    "AMI_Cmd.keystore.firstLastName"));
			if ((answer = reader.readLine()).length() > 0)
				dname = answer;
		} else {
			// Host type
			prtMessage(messages.getString(
			    "AMI_Cmd.keystore.hostName"));
			if ((answer = reader.readLine()).length() > 0)
				dname = answer;
		}

		prtMessage(messages.getString("AMI_Cmd.keystore.orgUnitName"));
		if ((answer = reader.readLine()).length() > 0)
		    orgu = answer;

		prtMessage(messages.getString("AMI_Cmd.keystore.orgName"));
		if ((answer = reader.readLine()).length() > 0)
		    org = answer;

		prtMessage(messages.getString("AMI_Cmd.keystore.cityName"));
		if ((answer = reader.readLine()).length() > 0)
		    st = answer;

		prtMessage(messages.getString("AMI_Cmd.keystore.cntryName"));
		if ((answer = reader.readLine()).length() > 0)
		    co = answer;

		nameDN = "CN=" + dname + ", OU=" + orgu + ", O=" + org +
		", L=" + st + ", C=" + co;
	    }

	    {
	        Object[] args = { new String(nameDN) };
	        msgFormatter.applyPattern(
		    messages.getString("AMI_Cmd.keystore.nameDNok"));
	        prtMessage(msgFormatter.format(args));
	    }
	    answer = reader.readLine().toLowerCase();
	    if (answer.length() > 0 && !answer.startsWith("yes"))
	    	    nameDN = null;
	  } catch (IOException e) {
	    	prtError(messages.getString("AMI_Cmd.keystore.noNameDN"));
		prtVerbose(e.getMessage());
		exit(1);
	  }
	} while (nameDN == null);
	return nameDN;
    }

    public String getNameDNS()   {
       // Get current users dns name 
	if (nameDNS != null)
		return (nameDNS);

	try {
		nameDNS = AMI_KeyMgntClient.getDNSNameFromLoginName(
		    _name, amiType);
		if (nameDNS != null)
			return (nameDNS);
	} catch (Exception e) {
		nameDNS = null;
	}

	// Input reader
	BufferedReader reader = new BufferedReader(
	    new InputStreamReader(System.in));

	// Get the DNS Name from the user
	System.out.println("\n" + messages.getString(
	    "AMI_Cmd.admin.promptemail"));
	try {
		nameDNS = reader.readLine();
	} catch (IOException e) {
		e.printStackTrace();
		exit(-1);
	}
	return nameDNS;
    }

    // Internationalization
    private static ResourceBundle   messages;
    private static MessageFormat    msgFormatter;

    private String subcmd = "list";
    private int keysize = 512;
    private String keyAlgName = "RSA";
    private String sigAlgName = null;
    private int valDays = 365;
    private String keyalias = null;
    private String _name = System.getProperty("user.name");
    private String nameDN = null;
    private String nameDNS = null;
    private String keypass = null;
    private String keystorefile = null;
    private String amiType = AMI_Constants.AMI_USER_OBJECT;
    private String filename = null;
    private String bulkFile = null;
    private String oldpasswd = null;
    private String newpasswd = null;
    private boolean _verboseMode = false;
    private boolean _silentMode = false;
    private InputStream _input = (InputStream) System.in;
    private OutputStream _output = (OutputStream) System.out;
    private String hostIP = null;
    private boolean rfc = true;
    private static final int KEYENTRY		= 0;
    private static final int TRUSTEDCACERTS	= 1;
    private static final int TRUSTEDCERT	= 2;
    private int entry_type = KEYENTRY;
    private static String beginCertificate = "-----BEGIN CERTIFICATE-----\n";
    private static String endCertificate = "-----END CERTIFICATE-----\n";
    private static Hashtable _subcmds = null;

}
