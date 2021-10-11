/*
 * Copyright(c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_EncryptCmd.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.cmd;

import java.lang.*;
import com.sun.ami.utils.AMI_Utils;
import java.text.*;
import java.util.*;
import java.io.*;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.security.*;
import sun.security.util.DerOutputStream;
import java.security.cert.*;
import java.security.cert.X509Certificate;
import java.security.cert.Certificate;
import sun.misc.BASE64Encoder;
import sun.security.pkcs.*;
import sun.security.x509.X500Name;
import sun.security.util.BigInt;
import sun.security.util.DerValue;

import com.sun.ami.AMI_Exception;
import com.sun.ami.utils.AMI_Utils;
import com.sun.ami.keymgnt.AMI_KeyMgntClient;
import com.sun.ami.keymgnt.AMI_KeyStore_Certs;
import com.sun.ami.keygen.AMI_VirtualHost;
import com.sun.ami.common.*;
import com.sun.ami.crypto.AMI_Crypto;
import com.sun.ami.keygen.AMI_KeyGen;
import com.sun.ami.crypto.AMI_CryptoException;
import com.sun.ami.ca.*;
import com.sun.ami.utils.AMI_GetOpt;
//import com.sun.ami.utils.AlgorithmId;


/**
* This class implements amiencrypt command.

* @author Bhavna Bhatnagar since February'99
*
* @version 1.0
*
*/

public class AMI_EncryptCmd extends AMI_Common {

private String DEFAULT_WRAP_ALGO = "RSA";
private String DEFAULT_CIPHER_FILENAME = "cipherdata";
private String DEFAULT_CIPHER_EXTN = ".cipherdata";
private int AMI_END_DATA = 2;
private int AMI_IV_SIZE = 8;
private String BEGIN_CIPHER_DATA_ONLY = "-----BEGIN ENCRYPTED DATA-----";
private String END_CIPHER_DATA_ONLY = "-----END ENCRYPTED DATA-----";
/* Mask to check for parity adjustment */
private static final byte[] PARITY_BIT_MASK = {
(byte)0x80, (byte)0x40, (byte)0x20, (byte)0x10,
(byte)0x08, (byte)0x04, (byte)0x02
};
private static  AMI_Utils util = new AMI_Utils();

public AMI_EncryptCmd() {
}

/*
* Parse command line options
*/
private void parseOption(String[] argv) throws Exception {

AMI_GetOpt go = new AMI_GetOpt(argv, "bsvl:i:o:hL:r:xc:k:");
	int ch = -1;
	boolean isVirtualHost = false;
	String filename = null;
	String hostIP = null;
	boolean rootOpt = false;
	String outputFile = null;
	String cipherFile = DEFAULT_CIPHER_FILENAME;

	/**
	 * Parse and take action for each specified property.
	 */
/*
	if (argv.length == 0) {
		Usage();
		exit(1);
	}
*/
	try {
	  while ((ch = go.getopt()) != go.optEOF) {

	    if ((char)ch == 'b') 
		_prtEncryptBoundry = true;
	        else
	        if ((char)ch == 'c') {
		     try {
		       filename = go.optArgGet();
		       _cos = new PrintStream(new FileOutputStream(filename));
		   } catch (IOException e) {
		       Object[] args = { new String(filename) };
		       msgFormatter.applyPattern(
			   messages.getString("AMI_Cmd.io"));
		       prtError(msgFormatter.format(args));
		       exit(1);
		   }
	        }
	        else
	        if ((char)ch == 'h') {
		   if (!System.getProperties().getProperty("user.name").
		   equals("root")) {
		       prtError(messages.getString("AMI_Cmd.encrypt.rootonly"));
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
			   hostIP = InetAddress.getByName(virtual).
			   getHostAddress();
		       } else {
			   hostIP = virtual;
			   _name = InetAddress.getByName(hostIP).getHostName();
		       }

		       AMI_VirtualHost.setHostIP(hostIP);
		       isVirtualHost = true;
	        }
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
			_fis = new FileInputStream(filename);
		   } catch (FileNotFoundException e) {
		       Object[] args = { new String(filename) };
		       msgFormatter.applyPattern(
			   messages.getString("AMI_Cmd.fileNotFound"));
		       prtError(msgFormatter.format(args));
		       exit(1);
		   }
	        }
	        else
	        if ((char)ch == 'k') 
		       _algorithm = go.optArgGet();
	        else
	        if ((char)ch == 'l') 
		       _keySize = (Integer.parseInt(go.optArgGet())); // In bits
	        else
	        if ((char)ch == 'o') {
		       try {
		        outputFile = go.optArgGet();
			_fos = new PrintStream(
			new FileOutputStream(outputFile));
		       } catch (IOException e) {
		           Object[] args = { new String(outputFile) };
			   msgFormatter.applyPattern(
			       messages.getString("AMI_Cmd.io"));
			   prtError(msgFormatter.format(args));
			   exit(1);
		       }
	        } 
	        else
	        if ((char)ch == 'r') 
		       _recipientCertsFile = go.optArgGet();
	        else
	        if ((char)ch == 's') 
		       _silentMode = true;
	        else
	        if ((char)ch == 'v') 
		       _verboseMode = true;
	        else
	        if ((char)ch == 'x') 
		       _external = true;
	        else {
		       Usage();
		       exit(1);
	        }     	
	   } // while 
	} catch (ArrayIndexOutOfBoundsException e) {
		Usage();
		exit(1);
	}

	for (int k = go.optIndexGet(); k < argv.length; k++) 	  
		_recipients.addElement(new String(argv[k]));

	if (_verboseMode)
		System.out.println("encyptCmd:: Parsed all options");
	/*
	 * Check if algo is valid 
	 */
	if (!algorithmSupported(_algorithm)) {
	    Object[] args = { new String(_algorithm) };
	    msgFormatter.applyPattern(
		messages.getString("AMI_Cmd.encrypt.algorithm"));
	    prtError(msgFormatter.format(args));
	    exit(1);
	}
	/*
	 * A root user has to explicitly specify -h to run.
	 */
	if (System.getProperties().getProperty("user.name").equals("root") 
	    && (_type != AMI_Constants.AMI_HOST_OBJECT)) {
	    prtError(messages.getString("AMI_Cmd.encrypt.isrootuser"));
	    exit(1);
	}

	// If no cipher file specified and external is true, use defaults
	if (_external && _cos == null)
	{
		if (outputFile != null) 
		    cipherFile = outputFile + DEFAULT_CIPHER_EXTN;

	        try {
		    _cos = new PrintStream(new FileOutputStream(cipherFile));
		} catch (IOException e) {
	    	    Object[] args = { new String(cipherFile) };
	    	    msgFormatter.applyPattern(
			messages.getString("AMI_Cmd.io"));
	    	    prtError(msgFormatter.format(args));
		    exit(1);
		}
}
}
private void run() throws Exception {

// Generate Symmetric Key 
byte[] symmetricKey = generateSymmetricKey();
	Certificate[] certsFromFile = null;
	Certificate[] certsForOtherRecipients = null;
	Certificate[] allRecipientCerts = null;
	KeyStore keystore = null;
	byte[] wrappedKey = null;
	AMI_RecipientInfo[] recInfo = null;
	byte[] encryptedData = null;
	int numCerts = 0;

	_keystore = KeyStore.getInstance("amiks", "SunAMI");
	_keystore.load(null, null);


	// Read Recipient Certs from file
	if (_recipientCertsFile != null) {
		Vector certsvec = readCertificateFile();
		certsFromFile = new Certificate[certsvec.size()];
		certsvec.copyInto(certsFromFile);
		numCerts = certsFromFile.length;
	}

	/* 
	* Get certificate for each receipient specified on command line
	* Choose an RSA certificate which verifies successfully from our 
	* trusted lists. 
	*/
	if (_recipients.size() != 0) {
		certsForOtherRecipients = getCertificatesForRecipients();
		numCerts += certsForOtherRecipients.length;
	}

	// If no receipient specified, get our own certificate  
	if (certsFromFile == null && certsForOtherRecipients == null) {
	    if (_verboseMode) {
	        System.out.print("encyptCmd:: No recipents specified ");
                System.out.println(".. encrypting for oneself");
            }

	    allRecipientCerts = new Certificate[1];
	    numCerts = 1;
	    
	    if ((allRecipientCerts[0] = _keystore.getCertificate(
		AMI_KeyMgntClient.getRSAEncryptAlias(_name, _type))) == null)
	    {
	        Object[] args = { _name };
	        msgFormatter.applyPattern(
		messages.getString("AMI_Cmd.encrypt.noCertForRecipient"));
	            prtError(msgFormatter.format(args));

	        throw new AMI_Exception(msgFormatter.format(args));
	    }
	} else {
	   // Make one array of certs	  
	   allRecipientCerts = new Certificate[numCerts];

	   if (certsFromFile != null) {
	        System.arraycopy(certsFromFile, 0, allRecipientCerts, 0,
		certsFromFile.length);
	  
	        if (certsForOtherRecipients != null)
		    System.arraycopy(certsForOtherRecipients, 0,
		    allRecipientCerts, certsFromFile.length,
		    certsForOtherRecipients.length);
	   }
	   else
	        if (certsForOtherRecipients != null)
	            System.arraycopy(certsForOtherRecipients, 0,
		    allRecipientCerts, 0, certsForOtherRecipients.length);
	  
	 }
				       
	// Create AMI_RecipientInfo objects
	recInfo = new AMI_RecipientInfo[allRecipientCerts.length];
	for (int ii = 0; ii < allRecipientCerts.length; ii++) {
	    if (_verboseMode) {
		System.out.print("encyptCmd:: ");
                System.out.println(allRecipientCerts.length + " Recipients ");
	    }
	    // Wrap the key 
	    wrappedKey = wrapKey(symmetricKey,
	    (X509Certificate)allRecipientCerts[ii]);
	    recInfo[ii] = new AMI_RecipientInfo(
	    new X500Name(((X509Certificate)allRecipientCerts[ii]).
	    getIssuerDN().getName()),
	    new BigInt(((X509Certificate)allRecipientCerts[ii]).
	    getSerialNumber()), _algorithmId, wrappedKey);
	 }

	// Encrypt Data 
	if (numCerts > 0) {
	 	encryptedData = encrypt(symmetricKey);

	 	// Write Output to appropriate files 
	 
	 	writeOutputFiles(encryptedData, recInfo);
	}

}

private byte[] encrypt(byte[] symmetricKey) throws AMI_Exception,
AMI_CryptoException, IOException
{

// Read input data from file 

	if (_verboseMode) 
		System.out.println("encyptCmd:: Encrypting the data");

	   
	byte[] input = util.InputStreamToByteArray(_fis);
	int inputLength = input.length;
	if (input == null) 
		throw new AMI_Exception("Error reading input data from file");

	// Call encrypt JNI
	AMI_Crypto crypto = new AMI_Crypto();

	// Encrypting

	crypto.ami_init();
	if (_algorithm.equalsIgnoreCase("RC4"))
		crypto.ami_rc4_encrypt(input, inputLength, AMI_END_DATA,
		symmetricKey, symmetricKey.length);

	else if (_algorithm.equalsIgnoreCase("RC2"))
	        crypto.ami_rc2_encrypt(input, inputLength, AMI_END_DATA,
		symmetricKey, symmetricKey.length, (symmetricKey.length) * 8,
		_iv);
				      
	else if (_algorithm.equalsIgnoreCase("DES"))
	        crypto.ami_des3des_encrypt(
		"DES", input, inputLength, AMI_END_DATA, symmetricKey, _iv);
				      
	else if (_algorithm.equalsIgnoreCase("3DES"))
	        crypto.ami_des3des_encrypt("3DES", input, inputLength,
		AMI_END_DATA, symmetricKey, _iv);

	crypto.ami_end();
	// Return the encrypted data
	return crypto.getOutputData();
}


private byte[] wrapKey(byte[] symmetricKey, X509Certificate cert)
throws AMI_Exception, AMI_CryptoException
{
	byte[] wrapped = null;
	AMI_Crypto crypto = new AMI_Crypto();
	if (_verboseMode)
	        System.out.println("encyptCmd:: Wrapping data ");
	byte[] publicKey =  cert.getPublicKey().getEncoded();

	if (_verboseMode)
	        System.out.println("encyptCmd:: got public key ");
	crypto.ami_rsa_wrap(symmetricKey, symmetricKey.length,
	DEFAULT_WRAP_ALGO, publicKey, publicKey.length, DEFAULT_WRAP_ALGO);

	// Return the wrapped key
	return crypto.getOutputData();
}

private void writeOutputFiles(byte[] encryptedData,
AMI_RecipientInfo[] recInfo) throws Exception 
{
	AMI_PKCS7 pkcs7 = null;

	if (_external) {
		pkcs7 = new AMI_PKCS7(new AMI_EncryptedContentInfo(_algorithmId,
		null), recInfo);
	        // Write out cipher data in cipher file
		BASE64Encoder encoder = new BASE64Encoder();

	        if (_prtEncryptBoundry)
		   _cos.println(BEGIN_CIPHER_DATA_ONLY);
	        encoder.encodeBuffer(encryptedData, _cos);

	        if (_prtEncryptBoundry)
		   _cos.println(END_CIPHER_DATA_ONLY);
	       
	        // Write out info data in output file 
	        pkcs7.printEnvelopeOnly(_fos, _prtEncryptBoundry);

	} else {
	       	pkcs7 = new AMI_PKCS7(new AMI_EncryptedContentInfo(_algorithmId,
		encryptedData), recInfo);
	        // Write out info data and cipher data in output file 
	        pkcs7.printEnvelopeAndData(_fos, _prtEncryptBoundry);
	}	       		     
}

private Certificate[] getCertificatesForRecipients() throws Exception {
	boolean verified = false;
	Vector certVec = new Vector();
	Object[] trustedKeys = getTrustedKeys();
	 
	if (_verboseMode) {
	    System.out.print("encyptCmd:: Getting certificate for others :");
	    System.out.println(_recipients.size());
        }

	for (int ii = 0; ii < _recipients.size(); ii++) {
		// Get all certificates for name
		verified = false;

	     	Certificate[] recCerts = _certsKeyStore.engineGetCertificates(
		(String)_recipients.elementAt(ii));
	     
	        if (recCerts == null) {
	    	    Object[] args =
		    { new String((String)_recipients.elementAt(ii)) };
	    	    msgFormatter.applyPattern(messages.getString(
		    "AMI_Cmd.encrypt.noCertForRecipient"));
	    	    prtError(msgFormatter.format(args));
	     	} else {

	     	    for (int jj = 0; jj < recCerts.length; jj++) {
	                if (recCerts[jj].getPublicKey().getAlgorithm().
		        equalsIgnoreCase("RSA")) {
		    	    if (verifyCertEstChain((
			    X509Certificate)recCerts[jj], trustedKeys)) {
			        certVec.addElement(recCerts[jj]);
			        verified = true;
			        break;
		    	    }
	  	        }		  
     	            }
                }
		if (!verified) {
	    	    Object[] args =
			{ new String((String)_recipients.elementAt(ii)) };
	    	    msgFormatter.applyPattern(messages.getString(
		    "AMI_Cmd.encrypt.notrustedcertsuser"));
		    prtError(msgFormatter.format(args));
	     	}
	}

	Certificate[] certs = new Certificate[certVec.size()];
	certVec.copyInto(certs);
	return certs;          
}


private boolean verifyCertEstChain(X509Certificate cert, Object[] trustedKeys) 
throws Exception
{
	boolean verified = false;

	if (_verboseMode)  {
		System.out.print("encyptCmd:: verifying cert est chain for");
		System.out.println(cert.getSubjectDN().getName());
	}    

	if (cert.getIssuerDN().equals(cert.getSubjectDN())) {
		try {
		    cert.verify(cert.getPublicKey());
	        } catch (Exception e) {
			if (_verboseMode) 
				System.out.println(e.getMessage());
			verified = false;
			return false;
    		}		        
		// if verifies sucessfully, check if this cert is in
		// our trusted list
		if (isTrustedKey((X509Certificate)cert, trustedKeys)) {
			verified =  true;
			return verified;
		}
	} else {
		Certificate[] issCerts = _certsKeyStore.engineGetCertificates(
		cert.getIssuerDN().getName());
	        for (int ii = 0; ii < issCerts.length; ii++) {
			try {
		        	cert.verify(issCerts[ii].getPublicKey());
		   	} catch (Exception e) {
				if (_verboseMode) 
					System.out.println(e.getMessage());
		       		verified =  false;
				continue;
		   	}
// If verifies, check if issuer is trusted

			if (isTrustedKey((X509Certificate)issCerts[ii],
			trustedKeys)) {
		        	verified = true;
		        	return verified;
			}
		   	else
		        if (!verifyCertEstChain((X509Certificate)issCerts[ii],
		   	trustedKeys)) {
		            verified = false;
			    continue;
			}
	      	}
	}
	return verified;
}


private boolean isTrustedKey(X509Certificate cert, Object[] trustedkeys)
throws Exception
{
	boolean found = false;

	if (_verboseMode)
	   System.out.println("Checking for trusted key ");
	for (int ii = 0; ii < trustedkeys.length; ii++) {

	    X509Certificate trustedCert = (X509Certificate)trustedkeys[ii];
	    if (cert.getSubjectDN().getName().equalsIgnoreCase(
	        trustedCert.getSubjectDN().getName())) {
		    trustedCert.checkValidity();
		    byte[] pkey1 = cert.getPublicKey().getEncoded();
		    byte[] pkey2 = trustedCert.getPublicKey().getEncoded();

		    if (pkey1.length != pkey2.length) {
			found = false;
			if (_verboseMode)
				System.out.println("Keys length mismatch");
			return found;
		    }
		    for (int jj = 0; jj < pkey1.length; jj++)
		        if (pkey1[jj] != pkey2[jj]) {
			    found = false;
			    if (_verboseMode)
				System.out.println("Keys mismatch");
			    return found;
		    }

		    if (_verboseMode)
		    	System.out.println("Key is trusted !");
		    found = true;	
		    return true;
	    }
	}

	return found; 
}


private Object[]  getTrustedKeys() throws Exception
{
	if (_verboseMode)
		System.out.println("Getting trusted keys..");
	Vector trustedCerts = new Vector();
	Enumeration enum = _keystore.aliases();

	while (enum.hasMoreElements()) {

	    String alias = (String)enum.nextElement();

	    if (_keystore.isCertificateEntry(alias)) {
	        trustedCerts.addElement(_keystore.getCertificate(alias));
	    }	     
	}
	if (_verboseMode)
	System.out.println("Number of trusted keys = " + trustedCerts.size());
	if (trustedCerts.size() == 0)
	    throw new AMI_Exception(messages.getString(
	    "AMI_Cmd.encrypt.notrustedcerts"));

	return trustedCerts.toArray();
}


private Vector readCertificateFile() throws Exception {
	Vector certs = util.LoadCert(_recipientCertsFile, null, null, 0);
	Vector verifiedCerts = new Vector();
	if (_verboseMode)
		System.out.println("read certs: "+certs.size());
	int totcerts = 0;
	Object[]  trustedKeys = getTrustedKeys();
	for (int i = 0; i < certs.size(); i++) {
		String algo = ((X509Certificate)(certs.elementAt(i))).
		getPublicKey().getAlgorithm();
		if (algo.equalsIgnoreCase("RSA"))  {
	  		if (verifyCertEstChain((X509Certificate)certs.
			elementAt(i), trustedKeys)) {
	     			verifiedCerts.addElement(certs.elementAt(i));
				totcerts++;
	     			continue;
	  		} else  {
				X509Certificate x509cert =
				(X509Certificate)certs.elementAt(i);
	     			Object[] args =
				{ new String((String)(x509cert.getSubjectDN().
				getName())) };
	     			msgFormatter.applyPattern(messages.getString(
				"AMI_Cmd.encrypt.notrustedcertsuser"));
				prtError(msgFormatter.format(args));
			}

		}		  
	}
	if (verifiedCerts.isEmpty()) {
		String str = messages.getString(
		"AMI_Cmd.encrypt.notrustercertsatall");
		throw new AMI_Exception(str);
	}
	return verifiedCerts;

}
private byte[] generateSymmetricKey() throws Exception {

	if (_verboseMode) {
	    System.out.print("encryptCmd:: Generating Symmetric Key for : ");
            System.out.println(_algorithm);
	}

	byte[] symmetricKey = null;
	if (_algorithm.equalsIgnoreCase("RC4")) {
	    SecureRandom random = SecureRandom.getInstance("SHA1PRNG");

	    symmetricKey = new byte[_keySize/8];
	    random.nextBytes(symmetricKey);
	} else if (_algorithm.equalsIgnoreCase("RC2")) {
	    SecureRandom random = SecureRandom.getInstance("SHA1PRNG");

	    symmetricKey = new byte[_keySize/8];
	    random.nextBytes(symmetricKey);

	    _iv = new byte[AMI_IV_SIZE];
	    random.nextBytes(_iv);

	    DerOutputStream out = new DerOutputStream();
	    DerOutputStream bytes = new DerOutputStream();

// Encode params 
	    bytes.putInteger(new BigInt(_keySize));
	    bytes.putBitString(_iv);
	    out.write(DerValue.tag_Sequence, bytes);

	    _algorithmId = new AlgorithmId("RC2",
	    new DerValue(out.toByteArray()));
	} else if (_algorithm.equalsIgnoreCase("DES")
	    || _algorithm.equalsIgnoreCase("3DES")) {
		AMI_KeyGen amiKgen = new AMI_KeyGen();
                if (_verboseMode)
		System.out.println("algo : "+_algorithm.toUpperCase());
		amiKgen.ami_gen_des3des_key(_algorithm.toUpperCase());
		symmetricKey = amiKgen.getSecretKey();
		_iv = amiKgen.getIV();
		_iv = new DerValue(_iv).getOctetString();
	    	DerOutputStream out = new DerOutputStream();
	    	DerOutputStream bytes = new DerOutputStream();

	// Encode params 
	    	bytes.putOctetString(_iv);
		out.write(DerValue.tag_Integer, bytes);
		if (_algorithm.equalsIgnoreCase("DES")) {
		    _algorithmId = new AlgorithmId("DES",
			new DerValue(out.toByteArray()));
                }
		if (_algorithm.equalsIgnoreCase("3DES")) {
		    _algorithmId = new AlgorithmId("3DES",
			new DerValue(out.toByteArray()));
                }
	
        }
// else do JNI for DES and 3DES XXXX 

	return symmetricKey;
}

private void Usage() {
	prtError(messages.getString("AMI_Cmd.encrypt.usage"));
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

private boolean algorithmSupported(String algo) throws Exception {
	if (algo.equalsIgnoreCase("DES") ||
	    algo.equalsIgnoreCase("3DES") ||
	    algo.equalsIgnoreCase("RC2") ||
	    algo.equalsIgnoreCase("RC4")) {

	    _algorithmId = AlgorithmId.get(algo);
	    return true;
	} else
	    return false;
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
	_certsKeyStore = new AMI_KeyStore_Certs();
}

/**
* Main method
*/
public static void main(String[] argv)  {
	AMI_EncryptCmd encrypt = new AMI_EncryptCmd();

	initialize();

	try {
	    encrypt.parseOption(argv);
	    encrypt.run();
	} catch (Exception e) {
	    System.out.println(e.toString());
	    encrypt.exit(1);
	}
}

// Internationalization
private static ResourceBundle   messages;
private static MessageFormat    msgFormatter;

private boolean         _prtEncryptBoundry = false;
private boolean         _verboseMode = false;
private boolean         _silentMode = false;
private boolean         _external = false;
private String          _algorithm = "RC4";
private InputStream     _fis = (InputStream) System.in;
private PrintStream    _fos = (PrintStream) System.out;
private PrintStream    _cos = null;
private String          _name = System.getProperties().getProperty("user.name");
private int             _keySize = 40;
private Vector          _recipients = new Vector(0, 1);
private String          _recipientCertsFile = null;
private AlgorithmId     _algorithmId = null;
private byte[]          _iv = null;
private String          _type = AMI_Constants.AMI_USER_OBJECT;
private KeyStore        _keystore = null;
private static AMI_KeyStore_Certs _certsKeyStore = null;
}
