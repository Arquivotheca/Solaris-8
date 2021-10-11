/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_Cert.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.cmd;
import java.lang.*;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.OutputStream;
import java.io.InputStream;
import java.io.DataInputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;

import java.text.MessageFormat;

import java.util.StringTokenizer;
import java.util.ResourceBundle;
import java.util.Locale;
import java.util.Enumeration;
import java.util.Vector;

import java.security.KeyStore;
import java.security.Security;

import sun.misc.BASE64Decoder;
import sun.misc.BASE64Encoder;

import java.security.KeyStoreException;
import java.security.NoSuchProviderException;
import java.security.NoSuchAlgorithmException;
import java.security.UnrecoverableKeyException;
import java.security.cert.CertificateException;
import java.security.cert.Certificate;
import java.security.cert.X509Certificate;
import java.security.cert.CertificateFactory;

import com.sun.ami.keymgnt.AMI_KeyMgntClient;
import com.sun.ami.keymgnt.AMI_KeyMgnt_FNS;
import com.sun.ami.keymgnt.AMI_KeyStore_Certs;
import com.sun.ami.common.SunAMI;
import com.sun.ami.common.AMI_Constants;
import com.sun.ami.common.AMI_Log;
import com.sun.ami.common.AMI_Common;
import com.sun.ami.utils.AMI_GetOpt;

import sun.security.x509.X509CertImpl;

import java.net.InetAddress;
import java.net.UnknownHostException;
import com.sun.ami.keygen.AMI_VirtualHost;

/**
 *
 * The following impliments the amicert command (for more information
 * see the amicert man page.
 *
 * @author  Paul Klissner
 *
 */

public class AMI_Cert extends AMI_Common {

	final static int IMPORT = 0;
	final static int EXPORT = 1;
	final static int REMOVE = 2;
	final static int DISPLAY = 3;

	final static int EXPECT_NOTHING = 0;
	final static int EXPECT_FILENAME = 1;
	final static int EXPECT_OWNER = 2;
	final static int EXPECT_ISSUER = 3;
	final static int EXPECT_SERNO = 4;

	final static int ERROR = -1;
	final static int SUCCESS = 0;

	/* Cmdline parser operates on these fields */
	static int subState = EXPECT_NOTHING;
	static int commandMode = DISPLAY;
	static boolean verboseMode = false;
	static boolean silentMode = false;
	static String owner = null;
	static String ownerCMD = null;
	static String ownerDN = null;
	public static String ownerType = AMI_Constants.AMI_USER_OBJECT;
	static String filename = null;
	static String issuerDN = null;
	static long serialNbr = 0;
	static byte binaryCert[];
	static KeyStore stashedKeyStore = null;
	static AMI_KeyStore_Certs stashedCertKeyStore = null;

	static ResourceBundle messages;
	static MessageFormat msgFormatter;

	static final String CERT_BEGIN = "-----BEGIN CERTIFICATE-----";
	static final String CERT_END = "-----END CERTIFICATE-----";

	public static void main(String args[]) throws Exception {

		/* Set the providers, if not already installed. */
		if (Security.getProvider("SunAMI") == null) {
			Security.insertProviderAt(new SunAMI(), 1);
		}
		initialize();
		parseCmdLine(args);

		if ((AMI_KeyMgnt_FNS.fns_get_euid(null) == 0) &&
		    (ownerType != AMI_Constants.AMI_HOST_OBJECT)) {
			System.out.println(
			    messages.getString(
			    "AMI_Cmd.cert.err_hflagrequired"));
			System.exit(1);
		}
		switch (commandMode) {
		case DISPLAY:
			if (!DisplayCert())
				System.out.println(
				    messages.getString(
				    "AMI_Cmd.cert.err_toofewcerts"));
			break;
		case IMPORT:
			ImportCert();
			break;
		case EXPORT:
			ExportCert();
			break;
		case REMOVE:
			RemoveCert();
			break;
		}
	}


	/**
         * DISPLAY CERTIFICATE(S)
         * This applies the search criteria specified on the command
         * line and displays one or more matching certificates.
         */
	private static boolean DisplayCert() {
		boolean returnStatus = false;
		InputStream certIn = null;
		Enumeration certList = null;
		Vector certs = new Vector(10);

		if (filename != null) {
			certs = LoadCert(filename, ownerCMD, issuerDN,
			    serialNbr);
		} else {
			certs = LookupCert(ownerCMD, issuerDN, serialNbr);
		}

		if (certs == null)
			return false;

		/*
		 * Display the whole collection of matching certificates
		 */

		certList = certs.elements();

		logmsg("AMI_Cmd.cert.displaycert");
		while (certList.hasMoreElements()) {
			returnStatus = true;
			System.out.println(((X509CertImpl)
			    certList.nextElement()).toString());
		}
		return (returnStatus);
	}


	/**
         * IMPORT CERTIFICATE
         * Process Import option by loading the certificate from
         * the specified source, parsing it and saving it
         * in the keystore.
         */
	private static void ImportCert() {
		// Get the certificates in the file
		if (filename == null) {
			abort(null, messages.getString(
			    "AMI_Cmd.cert.filenotfound"));
		}

		/*
		 * Load certificate from file or stdin
		 */
		Vector newCerts = LoadCert(filename, ownerCMD,
		    issuerDN, serialNbr);
		if ((newCerts == null) || (newCerts.size() == 0)) 
			abort(null, messages.getString(
			    "AMI_Cmd.cert.err_certread"));

		/*
		 * Insert the certificate into the "certs" keystore
		 */
		try {
			AMINames names = new AMINames();
			Enumeration enum = newCerts.elements();
			X509Certificate theCert;
			long serNbr;
			String IssuerDN;
			while (enum.hasMoreElements()) {
				theCert = (X509Certificate) enum.nextElement();
				AMI_KeyMgntClient.
				addX509Certificate(names,
			        owner, theCert, ownerType);
				serNbr = ((X509CertImpl)theCert).
				getSerialNumber().longValue();
				IssuerDN = ((X509CertImpl)theCert).
				getIssuerDN().toString();
				syslog(serNbr, IssuerDN, "AMI_Cert.import");
			}
		} catch (Exception e) {
			e.printStackTrace();
			abort(e, messages.
			getString("AMI_Cmd.cert.err_storecert"));
		}
		logmsg("AMI_Cmd.cert.certadded");
	}


	/**
         * EXPORT CERTIFICATE
         * The following routine will export a certificate to
         * either the specified file or standard out
         */
	private static void ExportCert() {
		BufferedWriter certOut = null;
		X509CertImpl theCert = null;
		String IssuerDN = null;
		long serNbr = 0;
		Vector certs = new Vector(10);
		BASE64Encoder base64 = new BASE64Encoder();
		OutputStream certOutputStream;

		certs = LookupCert(ownerCMD, issuerDN, serialNbr);
		if (certs == null)
			 return;

		if (certs.size() > 1)
			 abort(null, messages.getString(
		"AMI_Cmd.cert.err_toomanycerts"));
		else if (certs.size() < 1)
			 abort(null, messages.getString(
			"AMI_Cmd.cert.err_toofewcerts"));

		theCert = (X509CertImpl) certs.elementAt(0);
		serNbr = (theCert.getSerialNumber()).longValue();
		IssuerDN = theCert.getIssuerDN().toString();

		try {

			logmsg("AMI_Cmd.cert.writecert");

			/*
			 * DER-encode the X509 Certificate
			 */
			ByteArrayOutputStream derEncodedCert =
			new ByteArrayOutputStream();
			theCert.derEncode(derEncodedCert);

			/*
			 * Establish output stream target
			 */
			if (filename != null)
				 certOutputStream = new 
				FileOutputStream(filename);
			else
				 certOutputStream = System.out;

			certOutputStream.write((CERT_BEGIN + "\n").
			getBytes());
			base64.encode(derEncodedCert.toByteArray(),
			certOutputStream);
			certOutputStream.write(("\n" + CERT_END + "\n").
			getBytes());

			if (filename != null)
				certOutputStream.close();

		} catch (IOException e) {
			abort(e, messages.
			getString("AMI_Cmd.cert.err_writecert"));
		}

		syslog(serNbr, IssuerDN, "AMI_Cert.export");
	}


	/**
         * REMOVE CERTIFICATE
         * The following routine removes a certificate if it can locate
         * exactly one certificate matching the input criteria.
         * otherwise it displays an error message and exits.
         */

	private static void RemoveCert() {
		InputStream certIn = null;
		X509CertImpl theCert = null;
		String IssuerDN = null;
		Vector certs = new Vector(10);
		long delSerNbr = 0;
		KeyStore certKeyStore;

		if (filename != null)
			 certs = LoadCert(filename, ownerCMD, issuerDN,
			serialNbr);
		else
			 certs = LookupCert(ownerCMD, issuerDN,
			serialNbr);

		if ((certs == null) || (certs.size() < 1))
			 abort(null, messages.
			getString("AMI_Cmd.cert.err_toofewcerts"));
		else if (certs.size() > 1)
			 abort(null, messages.
			getString("AMI_Cmd.cert.err_toomanycerts"));

		theCert = (X509CertImpl) certs.elementAt(0);
		delSerNbr = (theCert.getSerialNumber()).longValue();
		IssuerDN = AMI_KeyMgntClient.normalizeDN(
		    theCert.getIssuerDN().toString());

		/*
		 * Now use a 'wildcard' search to retrieve all certs and remove
		 * the matching serial number from the returned vector
		 */
		certs = LookupCert(null, null, 0);
		if (certs == null)
			 return;

		X509CertImpl delCert;
		for (int i = 0; i < certs.size(); i++) {
			delCert = (X509CertImpl) certs.elementAt(i);
			if (((delCert.getSerialNumber()).longValue() ==
			delSerNbr) &&
			    IssuerDN.equals(AMI_KeyMgntClient.normalizeDN(
			    (delCert.getIssuerDN()).toString()))) {
				certs.removeElementAt(i);
				break;
			}
		}

		/*
		 * Now delete all certs from the current keystore
		 */
		try {
			stashedKeyStore.deleteEntry(null);
		} catch (Exception e) {
			abort(null, messages.
			getString("AMI_Cmd.cert.err_cantdelete"));
		}

		/*
		 * Now add back all certificates
		 */
		for (int i = 0; i < certs.size(); i++) {
			try {
				stashedKeyStore.setCertificateEntry(
				    null, (Certificate) (certs.elementAt(i)));
			} catch (Exception e) {
				abort(e, messages.getString(
				    "AMI_Cmd.cert.err_cantdelete"));
			}
		}
		logmsg("AMI_Cmd.cert.certremoved");
		syslog(delSerNbr, IssuerDN, "AMI_Cert.remove");
	}

	static Vector getRequiredCertificates(Vector inCerts, String certOwner,
	    String certIssuer, long certSerialNbr) {
		Vector certs = new Vector(10);
		X509CertImpl certificate;

		if (certSerialNbr == 0 && certOwner == null &&
		certIssuer == null) {
			// Request for all certificates
			return inCerts;
		}

		Enumeration inCert = inCerts.elements();
		boolean isMatch;
		String normalizedCertOwner = null, normalizedCertIssuer = null;
		if (certOwner != null)
			normalizedCertOwner = AMI_KeyMgntClient.
			normalizeDN(certOwner);
		if (certIssuer != null)
			normalizedCertIssuer = AMI_KeyMgntClient.
			normalizeDN(certIssuer);
		while (inCert.hasMoreElements()) {
			isMatch = true;
			certificate = (X509CertImpl) inCert.nextElement();
			if ((certSerialNbr != 0) &&
			    (certificate.getSerialNumber().longValue() !=
			    certSerialNbr)) {
				isMatch = false;
			}
			
			if (isMatch && (certOwner != null) &&
			    !normalizedCertOwner.equals(
				AMI_KeyMgntClient.normalizeDN(
			    certificate.getSubjectDN().toString()))) {
				isMatch = false;
			}
			if (isMatch && (certIssuer != null) &&
			    !normalizedCertIssuer.equals(
			    AMI_KeyMgntClient.normalizeDN(
			    certificate.getIssuerDN().toString()))) {
				isMatch = false;
			}

			if (isMatch)
				certs.add(certificate);
		}
		return (certs);
	}

	/**
         * LOOKUP CERT
         * This routine looks up one or more certificates matching one or
         * a combination of input criteria.  It returns an enumerated list.
         *
         * @parameter certOwner - Either a username or DN to search cert 
	 * Subject for
         * @parameter issuerDN  - The Issuer DN to search for
         * @parameter serno     - The Serial Number to match
         *
         */
	static Vector LookupCert(String certOwner, String certIssuer,
	    long certSerialNbr) {

		Certificate[] certificates = null;
		KeyStore certKeyStore = null;
		Vector allCerts = null;
		Vector certs;

		// First determine the DN name user is looking for
		String certOwnerDN = null;
		if (certOwner == null) {
			certOwnerDN = null;
		} else if (certOwner.indexOf('=') == -1) {
			try {
				certOwnerDN =
				    AMI_KeyMgntClient.getDNNameFromLoginName(
				    certOwner);
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

		/*
		 * Load Key Store
		 */
		certKeyStore = LoadKeyStore();

		/*
		 * Get certificate list
		 */
		try {
			certificates = stashedCertKeyStore.
			engineGetCertificates(
			    certOwner);
			if ((certificates == null) ||
			(certificates.length == 0))
				return null;
			for (int i = 0; i < certificates.length; i++) {
				if (allCerts == null)
					allCerts = new Vector(10);
				allCerts.add(certificates[i]);
			}
		} catch (Exception e) {
			abort(e, messages.getString(
			    "AMI_Cmd.cert.err_getcertchain"));
		}
		return (getRequiredCertificates(allCerts,
		certOwnerDN, certIssuer,
		    certSerialNbr));
	}


	private static byte[] LoadSingleCert(BufferedReader certIn)
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
         * LOAD CERTIFICATE
         * The following method loads BASE64 DER encoded certificate
         * into a byte array.
         */
	private static Vector LoadCert(String inputFile, String certOwner,
	    String certIssuer, long certSerialNbr) {
		BufferedReader certIn;
		byte binaryCert[] = null;
		Vector certs = new Vector(10);
		X509CertImpl certificate;

		// First determine the DN name user is looking for
		String certOwnerDN = null;
		if (certOwner == null) {
			certOwnerDN = null;
		} else if (certOwner.indexOf('=') == -1) {
			try {
				certOwnerDN =
				    AMI_KeyMgntClient.getDNNameFromLoginName(
				    certOwner);
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
				certs.add(certificate);
			}
			/* 
			* Memory mgt. should take care of the
			* reader 
			*/
			if (inputFile != null) {
				certIn.close();	
			}
		} catch (Exception e) {
			abort(e, messages.
			getString("AMI_Cmd.cert.err_certread"));
		}
		return (getRequiredCertificates(certs, certOwnerDN, certIssuer,
		    certSerialNbr));
	}


	/**
         * PARSE CERTIFICATE
         * This method takes the binaryCert byte-array created when
         * the KeyStore was loaded, and converts the certfificate data
         * in to an X509Certificate object.
         */
	private static X509CertImpl ParseCert(byte[] certBytes) {
		X509CertImpl theCert = null;
		ByteArrayInputStream certStream =
		    new ByteArrayInputStream(certBytes);

		try {
			theCert = (X509CertImpl) new X509CertImpl(certStream);
		} catch (Exception e) {
			abort(e, messages.
			getString("AMI_Cmd.cert.err_readcert"));
		}

		return theCert;
	}


	/**
         * LOAD KEYSTORE
         * This methode instanciates the KeyStore provider and loads it
         * returning a KeyStore object.
         */
	private static KeyStore LoadKeyStore() {
		KeyStore theKeyStore = null;

		if (stashedKeyStore != null)
			 return stashedKeyStore;

		try {
			logmsg("AMI_Cmd.cert.loadkeystore");

			/*
			 * Retrieve Public Key certfificate keystore
			 */
			try {
				theKeyStore = KeyStore.getInstance("amicerts",
				    "SunAMI");
			} catch (Exception e) {
				abort(e, messages.getString(
				    "AMI_Cmd.cert.err_getkeystore"));
			}
			theKeyStore.load(null, null);
		} catch (Exception e) {
			abort(e, messages.getString(
			    "AMI_Cmd.cert.err_getkeystore"));
		}

		stashedKeyStore = theKeyStore;
		stashedCertKeyStore = new AMI_KeyStore_Certs();
		return theKeyStore;
	}


	/* 
         * NORMALIZE DN
         * Do this by taking the DN, converting to uppercase
         * removing whitespace from edges, saving only the
         * CN, OU, O and C fields, sorting them into that order
         * and reassembling the DN without excess whitespace.
         *
         * Example:  O = SUN MICROSYSTEMS, cn = Paul Klissner,
	 * C = US, ou = engineering
         * Becomes:  CN = PAUL KLISSNER, OU = ENGINEERING, 
	 * O = SUN MICROSYSTEMS, C = US
         */
	/* 
		private static String normalizeDN(String dn) {
		String l = null, x = null, p1 = null, p2 = null, cn = null,
		 ou = null, o = null, c = null, normalized = null;
		StringTokenizer y = null;

		StringTokenizer t1 = new StringTokenizer(
		dn.toUpperCase(), ",", false);

		while (t1.hasMoreTokens()) {

			x = new String((t1.nextToken()).trim());

			y = new StringTokenizer(x, "=", false);

			if (y.hasMoreTokens())
				p1 = new String((y.nextToken()).trim());

			if (y.hasMoreTokens())
				p2 = new String((y.nextToken()).trim());

			if (p1.equals("CN"))
				cn = new String(p1 + "=" + p2);
			else if (p1.equals("OU"))
				ou = new String(p1 + "=" + p2);
			else if (p1.equals("O"))
				o = new String(p1 + "=" + p2);
			else if (p1.equals("C"))
				c = new String(p1 + "=" + p2);
			else if (p1.equals("L"))
				l = new String(p1 + "=" + p2);


		}

		 normalized = cn + "," + ou + "," + o + "," + c;
		return normalized;
	} 
	*/


	/**
         * The following method parses the command line, setting up
         * internal operating state for the command.
         *
         * @parameter args - command line argument array
         */

	private static void parseCmdLine(String[] args) throws Exception {

	        AMI_GetOpt go = new AMI_GetOpt(args, "vso:f:hL:IERi:n:");
		int ch = -1;

		owner = new String(System.getProperty("user.name"));

		while ((ch = go.getopt()) != go.optEOF) {

		    if ((char)ch == 'E') {
		        if (commandMode == DISPLAY)
			     commandMode = EXPORT;
			else
			     usage();
		    } else
		    if ((char)ch == 'R')  {
		        if (commandMode == DISPLAY)
			     commandMode = REMOVE;
                    else
                        usage();
		    } else
		    if ((char)ch == 'I') {
		         if (commandMode == DISPLAY)
			      commandMode = IMPORT;
			 else
			      usage();
		    } else
		    if ((char)ch == 'v')
		         verboseMode = true;
		    else if ((char)ch == 's')
		         silentMode = true;
		    else if ((char)ch == 'f')
		         filename = go.optArgGet();
		    else if ((char)ch == 'o')
		         ownerCMD = go.optArgGet();
		    else if ((char)ch == 'i')
		         issuerDN = go.optArgGet();
		    else if ((char)ch == 'n')
		         serialNbr = hex2dec(go.optArgGet());		   
		    else if ((char)ch == 'h') {
                         if (AMI_KeyMgnt_FNS.fns_get_euid(null) != 0) {
                            System.err.println(
                            "\n"+messages.getString(
				"AMI_Cmd.cert.err_mustberoot")+"\n");
                            System.exit(-1);
			 }
			 ownerType = AMI_Constants.AMI_HOST_OBJECT;
		    } else
		    if ((char)ch == 'L') {
			 String hostIP = null;			 
		         try {
			      String virtualHost = go.optArgGet();
			      if (virtualHost.indexOf(".") < 0) {
				    hostIP = InetAddress.getByName(virtualHost).
				             getHostAddress();
				    AMI_VirtualHost.setHostIP(hostIP);
			      } else {
				    hostIP = InetAddress.getLocalHost().
					getHostAddress();
			      }			      
			 } catch (UnknownHostException e) {
			   System.err.println("\n");
                           System.err.println(messages.
			   getString("AMI_Cmd.cert.err_unknownhost")
			   + "\n");
			   System.exit(-1);
			 }
			 owner = hostIP;
		    } else {
		         usage();
		    }
		} // while

		/*
		 * We would like to get the code to the point where the
		 * underlying logic for getDNNameFromLoginName() uses the
		 * 'alternate names' extension to X509 V3 certificates as a
		 * place to store the username, as a way of associating a UNIX
		 * user with a DN.
		 * 
		 * Until then, AMI underneath getDNNameFromLoginName() uses a
		 * heuristic to find certificates for a user based on finding
		 * the home directory of the user, and then locating the cert
		 * file for the user (currently defaults to $HOME/.certX509
		 * 
		 */
		try {
			ownerDN = AMI_KeyMgntClient.getDNNameFromLoginName(
			    owner, ownerType);
		} catch (Exception e) {
			abort(e, messages.
			getString("AMI_Cmd.cert.err_getownerDN"));
		}

	}


	/**
         * This method converts a hex longword text representation
         * to a decimal integer value. -pk
         */
	private static long hex2dec(String hexNbr) {
		int j = 0, n = 0;
		String adjhex = new String(hexNbr);

		adjhex = (adjhex.trim()).toLowerCase();
		adjhex = "00000000".substring(0, 8 - adjhex.length()) + adjhex;
		for (int i = 0; i < 8; i++) {
			char c = adjhex.charAt(i);
			if (c >= 'a' && c <= 'f')
				 j = 10 + (c - 'a');
			else
				 j = c - '0';
			n += j * (int) Math.pow((double) 2,
			    (double) 7 * 4 - i * 4);
		}
		 return n;
	}

	/**
         * Displays usage error message and exits
         *
         */
	private static void usage() {
		// System.out.println("Usage index: " + j);
		for (int i = 0; i < 11; i++)
			System.err.println(
			    messages.getString(
			    "AMI_Cmd.cert.usage" + new Integer(i)));
		System.exit(SUCCESS);
	}



	/**
         * Logs a message to stdout, but only as warranted by the
         * cmdline settings of the silent and verbose flags.
         *
         * @parameter msg - Message to display
         */
	private static void logmsg(String key) {
		if (!silentMode && verboseMode)
			System.err.println(messages.getString(key));
	}



	/**
         * Aborts operation due to an error
         *
         * . No output is generate if silent flag is set.
         * . The message passed to the method is displayed if silent flag false.
         * . If the verbose flag is true and the exception parameter
	 *   is not null,
         *   a stack traceback is displayed.
         *
         * @parameter Exception - Exception to traceback if 
	 * non-silent/verbose mode.
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

	/**
         *
         * Initialize internationalization.
         *
         */
	private static void initialize() {
		try {
			// Load libami.so.1
			new AMI_Common();
			Locale currentLocale = AMI_Constants.initLocale();
			msgFormatter = new MessageFormat("");
			msgFormatter.setLocale(currentLocale);
			messages = AMI_Constants.getMessageBundle(
			    currentLocale);
		} catch (Exception e) {
			System.err.println(e.getMessage());
			e.printStackTrace();
		}
	}

	/**
         * Log certificate request
         */
	private static void syslog(long certSN, String issuer, String code) {
		try {
			Object[] params = {new String(Integer.toHexString(
			    (int) certSN)), new String(issuer)};
			AMI_Log.writeLog(1, code, params);
		} catch (Exception e) {
			throw new RuntimeException(e.toString());
		}
	}

}
