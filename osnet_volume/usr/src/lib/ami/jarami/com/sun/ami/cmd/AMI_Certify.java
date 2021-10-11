/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_Certify.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.cmd;

import java.io.FileReader;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.ByteArrayOutputStream;
import java.io.ByteArrayInputStream;
import java.io.FileOutputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.math.BigInteger;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.Signature;
import java.security.Security;
import java.security.KeyStore;
import java.security.KeyStoreException;
import java.security.NoSuchProviderException;
import java.security.NoSuchAlgorithmException;
import java.security.UnrecoverableKeyException;
import java.security.cert.X509Certificate;
import java.security.cert.CertificateException;
import java.security.cert.Certificate;
import java.text.MessageFormat;
import java.util.ResourceBundle;
import java.util.Locale;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.Calendar;
import java.util.GregorianCalendar;
import java.util.Date;
import java.util.TimeZone;
import java.util.Vector;

import sun.security.util.DerValue;
import sun.security.util.DerInputStream;
import sun.security.x509.CertificateVersion;

import sun.misc.BASE64Decoder;
import sun.misc.BASE64Encoder;

import com.sun.ami.keymgnt.AMI_KeyMgntClient;
import com.sun.ami.common.SunAMI;
import com.sun.ami.common.AMI_Constants;
import com.sun.ami.common.AMI_Log;
import com.sun.ami.common.AMI_Common;
import com.sun.ami.ca.AMI_CertReq;
import com.sun.ami.ca.AMI_X509CertImpl;
import com.sun.ami.ca.AMI_X509CertInfo;
import com.sun.ami.ca.AMI_CertReqException;
import com.sun.ami.keygen.AMI_VirtualHost;
import com.sun.ami.utils.AMI_GetOpt;

/**
 * This class provides a command line interface to AMI to facilitate
 * the CA's role in accepting and verifying the signature of certificate
 * requests, and converting them to signed X509 V3 certificates.
 *
 * @author: Paul Klissner
 * 
 * @version 1.0
 *
 */

public class AMI_Certify extends AMI_Common {


    public static final int ERROR	= -1;
    public static final int SUCCESS	=  0;

    public static final boolean CERT_SUCCESS  = true;
    public static final boolean CERT_FAILED   = false;

    static ResourceBundle   messages;
    static MessageFormat    msgFormatter;

    // Cmdline parser operates on these fields
    static String  inputFile   = null;
    static String  outputFile  = null;
    static String  validityCommence = null;
    static String  validityExpire   = null;
    static Date    commence         = null;
    static Date    expire           = null;
    static boolean verboseMode = false;
    static boolean silentMode  = false;
    static String  keyAlias    = null;
    static Integer validity    = new Integer(365);
    static String  sigAlg;
  
    // Fields pertaining to CA's keystore
    static X509Certificate certCA;
    static PrivateKey privKeyCA = null;
    static KeyStore   keyStoreCA;
  
    // Fields related directly to certificate object/object generation
    static AMI_CertReq        certReqObj;
    static AMI_X509CertImpl   X509Certificate;
    static String certSN;
    static String certReqKeyAlg;
    static AMI_CertReq certreq = null;
  
    public static String hostType = 
		AMI_Constants.AMI_USER_OBJECT;
    public static String hostIP;
    public static String hostName;


    // Fields pertaining to loading certificate request
    public static byte binaryCertReq[];
    public static final String CERTREQ_BEGIN 
         = "-----BEGIN NEW CERTIFICATE REQUEST-----";
    public static final String CERTREQ_END   
         = "-----END NEW CERTIFICATE REQUEST-----";
  
    public static final String CERTIFICATE_BEGIN 
         = "-----BEGIN CERTIFICATE-----\n";
    public static final String CERTIFICATE_END   
         = "\n-----END CERTIFICATE-----\n";


    public static void main(String args[]) throws Exception {

     // Set the providers, if not already installed.
    if (Security.getProvider("SunAMI") == null) {
                Security.insertProviderAt(new SunAMI(), 1);
    }

    // The agenda...
    initialize();
    parseCmdLine(args);

    /*
    * Check validity parameters for correct usage
    */
    if (validity.intValue() != 0 && validityExpire   != null) usage();
    if (validity.intValue() != 0 && validityCommence != null) usage();
    if (validityCommence != null && validityExpire == null) usage();
    if (validityCommence == null && validityExpire != null) usage();

    /*
    * If validity is not provided in any form, set default to
    * one year
    */
    if (validity.intValue() == 0 && validityCommence == null
         && validityExpire == null)
                validity = new Integer(365);

    if (validityCommence != null)
             commence = getGeneralizedTime(validityCommence);

    if (validityExpire != null)
             expire   = getGeneralizedTime(validityExpire);


    loadCertReq();
    loadCAKeystore();
    genX509Cert();
    writeCert();
    }


    /*
    *
    * Initialize internationalization.
    *
    */
    private static void initialize() {

    try {
        Locale currentLocale = AMI_Constants.initLocale();
        msgFormatter = new MessageFormat("");
        msgFormatter.setLocale(currentLocale);
        messages = AMI_Constants.getMessageBundle(
		currentLocale);
    } catch (Exception e) {
	System.err.println(e.getMessage());
	e.printStackTrace();
    }

    try {
        hostIP = InetAddress.getLocalHost().getHostAddress();
    } catch (UnknownHostException e) {
	System.err.println(
	   "\n"+messages.getString("AMI_Cmd.certify.err_unknownhost")+"\n");
        System.exit(-1);
    }

   
    }




    /*
    * Write a Base64-encoded(DER-encoded(X509Certificate)) 
    * to the user selected output stream.
    *
    */

    private static void writeCert() {

    try {
        BASE64Encoder base64 = new BASE64Encoder();
        OutputStream   certOutputStream;


        logMsg("AMI_Cmd.certify.writecert");

        /*
        * DER-encode the X509 Certificate
        */

        ByteArrayOutputStream derEncodedCert = new ByteArrayOutputStream();
        X509Certificate.derEncode(derEncodedCert);

        /*
        * Establish output stream target
        */    
 
        if (outputFile != null) 
     	    certOutputStream = new FileOutputStream(outputFile);
        else
	    certOutputStream = System.out;

        certOutputStream.write(CERTIFICATE_BEGIN.getBytes());
        base64.encode(derEncodedCert.toByteArray(), certOutputStream);
        certOutputStream.write(CERTIFICATE_END.getBytes());

    } catch (IOException e) {
        abort(e, messages.getString("AMI_Cmd.certify.err_writecert"));
    }

    }


    /*
    *  Validate certfificate request and create X509 V3 certficate
    *
    *  @Exception CertficateException
    *
    */
    private static void genX509Cert() {
  
    AMI_X509CertInfo certCtx = new AMI_X509CertInfo();


    logMsg("AMI_Cmd.certify.gencert");

	
    /*
    * Create certificate context
    */

    try {
        certCtx.setVersion(CertificateVersion.V3);
        certCtx.setSerialNumber();

        if (validity.intValue() != 0)
            certCtx.setValidity(validity.intValue()*24*60*60);
        else {
            System.out.println("Setting Validity: "+
                commence.toString()+","+expire.toString());
            certCtx.setValidity(commence, expire);
        }

        certCtx.setIssuer(certCA.getIssuerDN().getName());
        certCtx.setAlgorithm(sigAlg);
        certCtx.setCertReqAttrs(certreq);

	/*
         * Get certificate serial number for logging purposes
 	 */

	try {
	  certSN = certCtx.get("serialNumber").toString();
	  certSN = certSN.substring(15, certSN.length()-1).trim();
	} catch (IOException e) {
	  abort(e, messages.getString("AMI_Cmd.certify.err_gencert"));
	}

        /*
         * Log certificate request 
         */

        try {
           Object[] params = {  new String(certreq.getSubject().toString()), 
			        new String(certSN) };
 
  	   AMI_Log.writeLog(1, "AMI_Certify.processcertreq", params);
        } catch (Exception e) {
	   throw new RuntimeException(e.toString());
        }

    } catch (CertificateException e) {
	logCertStatus(CERT_FAILED);
        abort(e, messages.getString("AMI_Cmd.certify.err_gencert"));
    } 

    /*
    * Create the certfificate object 
    */

   
    X509Certificate = new AMI_X509CertImpl(certCtx);

    try {
        X509Certificate.sign(privKeyCA, sigAlg, "SunAMI");
    } catch (Exception e) {
	logCertStatus(CERT_FAILED);
        abort(e, messages.getString("AMI_Cmd.certify.err_signcert"));
    }
     
    logCertStatus(CERT_SUCCESS);


    if (verboseMode && !silentMode) 
         System.out.println("Certificate: " + X509Certificate);
    }


/*
*
* This method accepts a boolean argument indicating whether
* to write a success or failure for certificate generation to the
* logfile.
*
* @parameter sucesss - flag
*/

    private static void logCertStatus(boolean success) {
    if (success) {
    /*
    * Write message to AMI logfile indicating successful certificate
    * generation along with the issuer DN
    */

        try {
            Object[] params = {  new String(certCA.getIssuerDN().getName()),
			       new String(certSN)}; 

  	    AMI_Log.writeLog(1, "AMI_Certify.certgenerated", params);
        } catch (Exception e) {
	  throw new RuntimeException(e.toString());
       	}
    } else {
        /*
        * Write message to AMI logfile indicating certificate generation
	* failed for subject DN
        */

        try {
            Object[] params = {  new String(certCA.getIssuerDN().getName()), 
			       new String(certSN) }; 

  	    AMI_Log.writeLog(1, "AMI_Certify.certfailed", params);
       	} catch (Exception e) {
	  throw new RuntimeException(e.toString());
       	}
    }
    }


/*
*  Get CA's keystore, obtaining the default private key and cert.
*
*/

    private static void loadCAKeystore() {
    String kAlias = null;
    boolean DSA_alg = false;

    try {

        logMsg("AMI_Cmd.certify.loadkeystore");

        /*
        *  Establish providers, if necessary
        */

        if (Security.getProvider("SunAMI") == null) 
	   Security.insertProviderAt(new SunAMI(), 1);

        /*
        *  Retrieve CA's(our) keystore
        */

        try {
           keyStoreCA = KeyStore.getInstance("amiks", "SunAMI");
        } catch (Exception e) {
	   e.printStackTrace();
        }

        keyStoreCA.load(null, null);

        /*
        * Make sure that the signature algorithm takes the appropriate
	* default based on the key type in the certificate request,
	* or, if a signature algorithm is specified on the command line,
        * be sure it is compatible with the one in the certificate request,
        * and if it isn't, abort with an error message.
        */

        if (certReqKeyAlg.equalsIgnoreCase("RSA")) {
	   if (sigAlg == null) {
	        sigAlg = new String("MD5/RSA");
	   } else if (sigAlg.toUpperCase().indexOf("DSA") != -1) {
		abort(null, messages.
		getString("AMI_Cmd.certify.err_algmismatch"));
           }
        } else if (certReqKeyAlg.equalsIgnoreCase("DSA")) {
	   if (sigAlg == null) {
	        sigAlg = new String("SHA1/DSA");
	   } else if (sigAlg.toUpperCase().indexOf("RSA") != -1) {
		abort(null, messages.
		getString("AMI_Cmd.certify.err_algmismatch"));
           }
        }
       
        if (keyAlias == null) {
            try {
	    	if (sigAlg.equalsIgnoreCase("MD5withRSA") ||
	            sigAlg.equalsIgnoreCase("MD5/RSA")    ||
	            sigAlg.equalsIgnoreCase("MD2withRSA") ||
	            sigAlg.equalsIgnoreCase("MD2/RSA")) {
	            kAlias = AMI_KeyMgntClient.getRSASignAlias(
			     System.getProperty("user.name"),
			     AMI_Constants.AMI_USER_OBJECT);
		    
		    DSA_alg = false;
	    } else if (sigAlg.equalsIgnoreCase("SHA1withDSA") ||
	               sigAlg.equalsIgnoreCase("SHA1/DSA")) {
	            kAlias = AMI_KeyMgntClient.getDSAAlias(
	                     System.getProperty("user.name"),
			     AMI_Constants.AMI_USER_OBJECT);
		    DSA_alg = true;
	    }
	  } catch (Exception e) {
	    if (verboseMode)
	        msgWithArgs("AMI_Cmd.certify.err_nosuchkey", kAlias);
	    abort(e, "");
	  }
	} else
	    kAlias = keyAlias;

        /*
        *  Get private key and public key(certificate)
        */

	if (! kAlias.equalsIgnoreCase("null")) {
           // Let the alias we use to be based on discerned based on either
	   // the alias provided on the command line or from the
	   // from the alias based on the key or signature algorithms
	   if (verboseMode) msgWithArgs("AMI_Cmd.certify.tryalias", kAlias);
       	} else { // otherwise, just try "mykey"
	   kAlias =  new String("mykey");
	   if (verboseMode) msgWithArgs("AMI_Cmd.certify.tryalias", kAlias);

	}

	// Now actually try to find the key 
       	if ((privKeyCA = (PrivateKey)keyStoreCA.getKey(kAlias, null))
	!= null) {
       	   // we found the key, now get the certificates
	   Certificate[] certs = keyStoreCA.getCertificateChain(kAlias);
	   certCA = (X509Certificate)certs[0];
       	} else {
	   // Haven't been able to find a default key alias,
	   // so now select last ditch aliases  (mykeyRSA and mykeyDSA)
	   // alias ..
		if (verboseMode)
	            msgWithArgs("AMI_Cmd.certify.err_nosuchkey", kAlias);
	        if (DSA_alg)  {		// try "mykeyDSA"
		    kAlias = "mykeyDSA";
		if (verboseMode)
		    msgWithArgs("AMI_Cmd.certify.tryalias", kAlias);
	        } else {
		    kAlias = "mykeyRSA";
		    if (verboseMode)
		        msgWithArgs("AMI_Cmd.certify.tryalias", kAlias);
	        }
	        // Look up key based on last-ditch alias
	        if ((privKeyCA = 
       	           // we found the key, now get the certificates
	       	   (PrivateKey) keyStoreCA.getKey(kAlias, null)) != null) {
		   Certificate[] certs = keyStoreCA.getCertificateChain(kAlias);
		   certCA = (X509Certificate)certs[0];
	        }	       
       	 }

       	 if (privKeyCA == null || certCA == null) {
	    if (verboseMode)
	        msgWithArgs("AMI_Cmd.certify.err_nosuchkey", kAlias);
	    abort(null, "");
       	 }
    } catch (Exception e) {
        abort(e, messages.getString("AMI_Cmd.certify.err_getkeystore"));
    } 

    }



/**
*  Load a BASE64 DER-encoded certficate request from input source
*
*/
 
    private static void loadCertReq() {

    BASE64Decoder base64 = new BASE64Decoder();
    StringBuffer certBuf = new StringBuffer();
    BufferedReader certIn;
    String line = null;

    logMsg("AMI_Cmd.certify.loadcertreq");
 
    try {

    /*
    * Establish input source of certificate request
    */

    if (inputFile != null) 
 	 certIn = new BufferedReader(new FileReader(inputFile));
    else
	 certIn = new BufferedReader(new InputStreamReader(System.in));

    if (certIn == null)  
	 abort(null, messages.getString("AMI_Cmd.certify.err_openinput")); 

    /*
    * Locate begin marker
    */
    line = certIn.readLine();
    while (line != null && !line.equals(CERTREQ_BEGIN)) {
	line = certIn.readLine();
    }
     
    if (line == null) 
        abort(null, messages.getString("AMI_Cmd.certify.err_invalidcert"));

    /* 
    * Collect lines into a single buffer until reaching end marker
    */
    line = certIn.readLine();
    while (line != null && !line.equals(CERTREQ_END)) {
	certBuf.append(line);
	line = certIn.readLine();
    }

    if (line == null)     
	abort(null, messages.getString("AMI_Cmd.certify.err_invalidcert"));
 
    /*
    * Convert certificate request from BASE64 encoded to binary
    */

    binaryCertReq = base64.decodeBuffer(certBuf.toString());

    certIn.close(); 

    /*
    * Parse certificate request and verify signature.
    */

    try {
        certreq = new AMI_CertReq(binaryCertReq);
    } catch (AMI_CertReqException e) {
	abort(e, messages.getString("AMI_Cmd.certify.err_parsecert"));
    }
     
    if (verboseMode && !silentMode) 
        System.out.println("CertReq: " + certreq);

    /*
    * Get the algorithm used for the key
    */
    certReqKeyAlg = certreq.getPublicKey().getAlgorithm();
     
    } catch (IOException e) {
	abort(e, messages.getString("AMI_Cmd.certify.err_certread"));
    }

    }


/*
* Parse command-line arguements and set operational context
*
* @parameter String[] args - Command line arguments
*
*/

    private static void parseCmdLine(String[] args) throws Exception {

        AMI_GetOpt go = new AMI_GetOpt(args, "vsV:k:a:i:o:hL:c:e:");
	boolean isVirtualHost = false;
        int ch = -1;

	while ((ch = go.getopt()) != go.optEOF) {

	    if ((char)ch == 'v') 
		    verboseMode = true;
	    else
	        if ((char)ch == 's') 
		    silentMode = true;
	    else
	        if ((char)ch == 'V') 
		    validity = new Integer(go.optArgGet());
	    else
	        if ((char)ch == 'k') {
		    String algo  = go.optArgGet();
		    if (algo.equalsIgnoreCase("MD5withRSA")
			|| algo.equalsIgnoreCase("MD5/RSA"))
		      sigAlg = new String("MD5withRSA");
		    else if (algo.equalsIgnoreCase("MD2withRSA")
			     || algo.equalsIgnoreCase("MD2/RSA"))
		      sigAlg = new String("MD2withRSA");
		    else if (algo.equalsIgnoreCase("SHA1withDSA")
			     || algo.equalsIgnoreCase("SHA1/DSA"))
		      sigAlg = new String("SHA1withDSA");
		    else
		      abort(null, messages.
			getString("AMI_Cmd.certify.err_unknownalg"));
	    }
	    else
	        if ((char)ch == 'a') 
		    keyAlias = go.optArgGet();
	    else
	        if ((char)ch == 'i') 
		    inputFile = go.optArgGet();
	    else
	        if ((char)ch == 'o') 
		    outputFile = go.optArgGet();
	    else
	        if ((char)ch == 'h') {
		    if (!(System.getProperty("user.name").equals("root"))) {
		          System.err.println(
			  "\n"+messages.
			getString("AMI_Cmd.certify.err_mustberoot")+"\n");
			  System.exit(-1);
		}

		hostType = AMI_Constants.AMI_HOST_OBJECT;
		if (!isVirtualHost)
		          hostName = InetAddress.getLocalHost().getHostName();
	    }
	    else
	        if ((char)ch == 'L') {
                    String virtualHost = go.optArgGet();
                    if (virtualHost.indexOf(".") < 0) {
                       hostName = virtualHost;
                       hostIP = InetAddress.getByName(hostName).
			getHostAddress();
                    } else {
                       hostIP = virtualHost;
                       hostName = InetAddress.getByName(hostIP).getHostName();
                    }
                    AMI_VirtualHost.setHostIP(hostIP);
		    isVirtualHost = true;
	    }
	    else
	        if ((char)ch == 'c') 
		    validityCommence = go.optArgGet();
	    else
	        if ((char)ch == 'e') 
		    validityExpire = go.optArgGet();
	    else
		    usage();		    
	}

	/*
	 * A root user has to explicitly specify -h to run.
	 */
	if (System.getProperty("user.name").equals("root") && 
	    hostType != AMI_Constants.AMI_HOST_OBJECT) {
	    abort(null, messages.getString("AMI_Cmd.certify.isrootuser"));
	}
    }
 
/*
* Display usage clause
*/ 
    private static void usage() {
          for (int i = 0; i < 10; i++)
                  System.err.println(
                      messages.getString(
                      "AMI_Cmd.certify.usage" + new Integer(i)));
          System.exit(SUCCESS);
    }
/*
* Logs a message to stdout, but only as warranted by the
* cmdline settings of the silent and verbose flags.
*
* @parameter msg - Message to display
*/
    private static void logMsg(String key) {

     	if (!silentMode && verboseMode) 
	    System.err.println(messages.getString(key));
    }
	

    public static void msgWithArgs(String s, String p1) {
   	try {
    		Object[] params = { new String(p1) };
    		System.err.println( 
      		msgFormatter.format(
         	messages.getString(s), params));
   	} catch (Exception e) {
      		throw new RuntimeException(e.toString());
   	}
    }

/*
* Aborts operation due to an error
*
* . No output is generate if silent flag is set.
* . The message passed to the method is displayed if silent flag false.
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
	    if (e != null && verboseMode) 
            	e.printStackTrace();
        }
        System.exit(ERROR);
    }

    public static Date getGeneralizedTime(String UTCtime) throws IOException {
    ByteArrayInputStream buffer = new ByteArrayInputStream(UTCtime.getBytes());

    int len = UTCtime.length();
    /*
    * Generalized time encoded as ASCII chars, YYYYMMDDhhmm[ss]
    */
    int year, month, day, hour, minute, second;

    year = 1000 * Character.digit((char)buffer.read(), 10);
    year += 100 * Character.digit((char)buffer.read(), 10);
    year +=  10 * Character.digit((char)buffer.read(), 10);
    year += Character.digit((char)buffer.read(), 10);

    month = 10 * Character.digit((char)buffer.read(), 10);
    month += Character.digit((char)buffer.read(), 10);
    month -= 1;     // Calendar months are 0-11

    day = 10 * Character.digit((char)buffer.read(), 10);
    day += Character.digit((char)buffer.read(), 10);

    hour = 10 * Character.digit((char)buffer.read(), 10);
    hour += Character.digit((char)buffer.read(), 10);

    minute = 10 * Character.digit((char)buffer.read(), 10);
    minute += Character.digit((char)buffer.read(), 10);

    len -= 12;
    /*
    * We allow for non-encoded seconds, even though the
    * IETF-PKIX specification says that the seconds should
    * always be encoded even if it is zero.
    */

    if (len == 3 || len == 7) {
        second = 10 * Character.digit((char)buffer.read(), 10);
        second += Character.digit((char)buffer.read(), 10);
        len -= 2;
    } else
        second = 0;

    if (month < 0 || day <= 0
              || month > 11 || day > 31 || hour >= 24
              || minute >= 60 || second >= 60)
                abort(null, "1"+messages.
		getString("AMI_Cmd.certify.err_invalidtime"));


    Calendar cal = Calendar.getInstance(TimeZone.getTimeZone("GMT"));
    cal.set(year, month, day, hour, minute, second);
    cal.set(Calendar.ERA, GregorianCalendar.AD);
    Date readDate = cal.getTime();
    long utcTime = readDate.getTime();
    /*
    * Finally, "Z" or "+hhmm" or "-hhmm" ... offsets change hhmm
    */
    if (! (len == 1 || len == 5))
        abort(null, "2"+messages.getString("AMI_Cmd.certify.err_invalidtime"));

    switch (buffer.read()) {
        case '+':
	{
        	int Htmp = 10 * Character.digit((char)buffer.read(), 10);
                Htmp += Character.digit((char)buffer.read(), 10);
                int Mtmp = 10 * Character.digit((char)buffer.read(), 10);
                Mtmp += Character.digit((char)buffer.read(), 10);

                if (Htmp >= 24 || Mtmp >= 60)
                    abort(null,
                       "3"+messages.
			getString("AMI_Cmd.certify.err_invalidtime"));

                utcTime += ((Htmp * 60) + Mtmp) * 60 * 1000;
        }
        break;

        case '-':
        {
                int Htmp = 10 * Character.digit((char)buffer.read(), 10);
                Htmp += Character.digit((char)buffer.read(), 10);
                int Mtmp = 10 * Character.digit((char)buffer.read(), 10);
                Mtmp += Character.digit((char)buffer.read(), 10);

                if (Htmp >= 24 || Mtmp >= 60)
                    abort(null,
                       "4"+messages.
			getString("AMI_Cmd.certify.err_invalidtime"));


                utcTime -= ((Htmp * 60) + Mtmp) * 60 * 1000;
        }
        break;

        case 'Z':
        break;

        default:
        	abort(null, "5"+messages.
		getString("AMI_Cmd.certify.err_invalidtime"));
    }

    readDate.setTime(utcTime);
    return readDate;
    }
}
