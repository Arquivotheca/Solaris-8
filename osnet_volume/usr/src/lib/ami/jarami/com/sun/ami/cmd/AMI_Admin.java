/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_Admin.java	1.3 99/07/23 SMI"
 */

package com.sun.ami.cmd;

import java.lang.*;
import java.text.*;
import java.util.*;
import java.io.*;
import java.net.InetAddress;
import java.net.UnknownHostException;
import sun.security.x509.*;

import com.sun.ami.AMI_Exception;
import com.sun.ami.keymgnt.AMI_KeyMgntClient;
import com.sun.ami.keymgnt.AMI_KeyMgntNotFoundException;
import com.sun.ami.keymgnt.AMI_KeyMgntService;
import com.sun.ami.common.AMI_Constants;
import com.sun.ami.common.AMI_Common;
import com.sun.ami.common.AMI_Log;
import com.sun.ami.keygen.AMI_VirtualHost;
import com.sun.ami.utils.AMI_GetOpt;

/**
 * This class performs the command line function 'amiadmin'.
 * With this command various user preferences can be changed,
 * such as the default keys, certificate, etc, and several
 * AMI system properties can be viewed and modified.
 *
 * @author   Paul Klissner
 *
 * @version  1.0
 *
 */

public class AMI_Admin {

	public static String PROPERTIES_FILE = "/etc/ami/ami.properties";
	public static String pad =
	new String("                                 ");

	public static final int USER_PREF = 0;
	public static final int SYSTEM_PROP = 1;

	/* Constants to label method indicies */
	public static final int DEFAULT_RSA_SIGNATURE_KEY = 0;
	public static final int DEFAULT_RSA_ENCRYPTION_KEY = 1;
	public static final int DEFAULT_DSA_KEY = 2;
	public static final int DEFAULT_DH_KEY = 3;
	public static final int KEYSTORE_FILE = 4;
	public static final int CERT_FILE = 5;
	public static final int CA_CERT_FILE = 6;
	public static final int CERT_CHAIN_FILE = 7;
	public static final int CERT_REQ_FILE = 8;
	public static final int BACKUP_CERT_REQ_CERT_FILE = 9;
	public static final int SET_NAME_DN = 10;

	/* Parser substates */
	public static final int RESET_SUBSTATE = 0;
	public static final int GET_PROP_TOKEN = 1;
	public static final int GET_VALUE_TOKEN = 2;
	public static final int GET_BACKEND = 3;

	static int assignState = RESET_SUBSTATE;

	/* Named booleans */
	public static final boolean PRINT_HEADER = true;
	public static final boolean SUPPRESS_HEADER = false;

	/* DN and DNS name of the user */
	public static String userDNName = null;
	public static String userDNSName = null;
	public static boolean initialized = false;

	static methodMap[] userPrefs = {
		new methodMap("defaultRSAsignaturekey",
		DEFAULT_RSA_SIGNATURE_KEY),
		new methodMap("defaultRSAencryptionkey",
		    DEFAULT_RSA_ENCRYPTION_KEY),
		new methodMap("defaultDSAkey", DEFAULT_DSA_KEY),
		new methodMap("defaultDHkey", DEFAULT_DH_KEY),
		// new methodMap("keystorefile", KEYSTORE_FILE),
		// new methodMap("certfile", CERT_FILE),
		// new methodMap("cacertfile", CA_CERT_FILE),
		// new methodMap("certchainfile", CERT_CHAIN_FILE),
		// new methodMap("certreqfile", CERT_REQ_FILE),
		// new methodMap("backupcertreqcertfile",
		// BACKUP_CERT_REQ_CERT_FILE),
		new methodMap("namedn", SET_NAME_DN)
	};

	static nameMap[] sysProps = {
		// new nameMap(true, "nsKeyStoreRSA", "ns.keystoreRSA"),
		// new nameMap(true, "nsKeyStoreDSA", "ns.keystoreDSA"),
		// new nameMap(true, "nsKeyStoreDH", "ns.keystoreDH"),
		// new nameMap(true, "nsCertReq", "ns.certreq"),
		// new nameMap(true, "nsCertX509", "ns.certX509"),
		// new nameMap(true, "nsCaCertX509", "ns.cacertX509"),
		// new nameMap(true, "nsCertChainX509", "ns.certchainX509"),
		// new nameMap(true, "nsBackupCerts", "ns.backupCertReqCerts"),
		// new nameMap(true, "nsObjectProfile", "ns.objectProfile"),

		// new nameMap("userPrefix", "ldap.ami.user.prefix"),
		// new nameMap("hostPrefix", "ldap.ami.host.prefix"),
		// new nameMap("applicationPrefix", "ldap.ami.application.prefix"),

		// new nameMap("ldapAdmin", "ldap.ami.security.principal"),
		// new nameMap("ldapPassword", "ldap.ami.security.credentials"),
		// new nameMap("ldapVersion", "ldap.ami.ldap.version"),
		// new nameMap("ldapServerURL", "ldap.ami.provider.url"),

		// new nameMap("loglevel", "ami.logging.level"),
		// new nameMap("logfile", "ami.logging.filename"),

		// new nameMap("debuglevel", "ami.debugging.level"),
		// new nameMap("debugfile", "ami.debugging.filename"),

		new nameMap("i18nLanguage", "ami.locale.language"),
		new nameMap("i18nCountry", "ami.locale.country")
	};

	static AMINames names;
	/* Implements interface */
	static Hashtable propHT;
	/* Contains parsed properties file */
	static Hashtable sysTagHT;
	/* Contains system name tags */
	static Vector printList;
	/* Parsed cmdline props to print */
	static Vector assignList;
	/* Parsed cmdline props to assign */
	static assignPair ap;
	static String s;

	public static String hostType = AMI_Constants.AMI_USER_OBJECT;
	public static String hostIP;
	public static String hostName;

	public static boolean allProps = true;

	/* Internationalization */
	static ResourceBundle messages;
	static MessageFormat msgFormatter;

	/**
         * This is the command line activated entry point.
         * It drives the basic agenda.
         *
         * @parameter args  -  Command line arguments.
         *
         */
	public static void main(String[] args) {
	try {
		/* Load libami.so.1 library */
		new AMI_Common();

		sysTagHT = new Hashtable();
		/* Maps visible names to actual properties */
		propHT = new Hashtable();
		/* Properties imported from ami.properties */
		printList = new Vector(100);
		/* Post - parser worklist of props to display */
		assignList = new Vector(100);
		/* Post - parser worklist of props to assign */


		initialize();
		/* Internationalization initialization */

		/* Parse Command line arguments */
		try {
			parse_cmdline(args);
		} catch (UnknownHostException h) {
			System.err.println("\n" + messages.getString(
			    "AMI_Cmd.admin.unknownHost") + "\n");
			System.exit(-1);
		} catch (Exception e) {
			/* e.printStackTrace(); */
			System.err.println("\n" +
			    messages.getString(
			    "AMI_Cmd.admin.failure") +
			    "\n");
			System.exit(-1);
		}

		load_properties();
		/* Read AMI properties from file into hashtable */

		if (args.length == 0) {
			print_all_props();
		} else {
		  if (allProps) 
			print_all_props();
		  
		  else {
		        try {
			      assign_values();
			} catch (Exception e) {
			      /* e.printStackTrace(); */
			      System.err.println("\n" +
				    messages.getString(
				    "AMI_Cmd.admin.failure") +
				    "\n");
			      System.exit(-1);
			}
			print_requested_props();
		  }
		}
	} catch (Exception e) {
	}
	}

	/**
         * This method drives the assimilation of the properties file
         */

	private static void load_properties() throws IOException {
		BufferedReader propfile = new BufferedReader(
		    new FileReader(PROPERTIES_FILE));
		build_hashtables(propfile);
		propfile.close();
	}


	/**
         *  This method parses the command line arguments and sets
         *  up the program operating context based on the parsing.
         *
         *  @parameter args   -  Command line args
         *
         */

	private static void parse_cmdline(String[] args)
	throws Exception {

	    AMI_GetOpt go = new AMI_GetOpt(args, "b:m:hL:d:e:");
	    int ch = -1;
	    boolean isVirtualHost = false;
	    String value = null;

	    while ((ch = go.getopt()) != go.optEOF) {

		if ((char)ch == 'b') {
		      allProps = false;
		      
		      for (int p = 0; p < sysProps.length; p++) {
			  if (sysProps[p].backendSelection) {
				ap = new assignPair();
				ap.property = new String(
					      sysProps[p].visibleName);
				ap.value = go.optArgGet();
			/*
			* Here 's where we check for
			* valid backend
			*/
			   if (!ap.value.equals("file")
			       && !ap.value.equals("ldap")
			       && !ap.value.equals("fns")
			       && !ap.value.equals("nisplus")
			       && !ap.value.equals("nis")) {
					System.err.println(
					"\n" +
					messages.getString(
					"AMI_Cmd.admin.backend") + "\n");
						throw new AMI_Exception();
			   }
			   assignList.addElement(ap);
			  }
		      }
		}
		else
		if ((char)ch == 'm') {
		      allProps = false;
		      ap = new assignPair();
		      /* Get the Property name */
		      ap.property = go.optArgGet();

		      if (go.optIndexGet() >= args.length) {
			    System.err.println("\n" +
					       messages.getString(
						 "AMI_Cmd.admin.novalue"));
			    System.err.println("\n" +
					       messages.getString(
			    "AMI_Cmd.admin.usage1") + "\n");
			    throw new AMI_Exception();
		      }

		// If the property starts with nsKeyStore make sure it does not 
		// have keyType 
		      if (ap.property.startsWith("nsKeyStore") &&
			  !ap.property.equals("nsKeyStore")) {
			    try {
			         Object[] params = {
				       new String(ap.property)
				 };
				 System.err.println(
				       msgFormatter.format(
				       messages.getString(
				       "AMI_Cmd.admin.unknownprop2"),
			      	       params));
			    } catch (Exception e) {
			         throw new RuntimeException(e.toString());
			    }
			    System.exit(-1);
			    return;
		      }
		      // Get the value 
		      ap.value = args[go.optIndexGet()];
		      value = ap.value;
		      go.incrementOptIndex();

		// If the property is to set the nameservice
	        // backend, make sure it is one of the 
	        // following: file, fns, ldap, nis or nisplus
		      if (ap.property.startsWith("ns") &&
			  !((ap.value.equals("file")) ||
			    (ap.value.equals("fns")) ||
			    (ap.value.equals("ldap")) ||
			    (ap.value.equals("nisplus")) ||
			    (ap.value.equals("nis")))) {
			    /* Not a valid name service */
			    System.err.println("\n" +
					   messages.getString(
					   "AMI_Cmd.admin.backend") +
					    "\n");
			    throw new AMI_Exception();
		      }
					
			/*
		      	* Intercept nsKeyStore parameter and split
		      	* it into three properties which is
		      	* currently represents cummulatively.
		      	*/
		      if (ap.property.equals("nsKeyStore")) {
			    ap.property = "nsKeyStoreDH";
			    assignList.addElement(ap);
			    ap = new assignPair();
			    ap.property = "nsKeyStoreDSA";
			    ap.value = value;
			    assignList.addElement(ap);
			    ap = new assignPair();
			    ap.property = "nsKeyStoreRSA";
			    ap.value = value;
			    assignList.addElement(ap);
			    assignState = RESET_SUBSTATE;
		      } else {
			    assignList.addElement(ap);
		      }
		} else
		if ((char)ch == 'h') {
		      if (!(System.getProperty("user.name").equals(
			  "root"))) {
			    System.err.println("\n" +
			       		     messages.getString(
			       		     "AMI_Cmd.admin.root2") + "\n");
			    throw new AMI_Exception();
		      }
		      hostType = AMI_Constants.AMI_HOST_OBJECT;
		      if (isVirtualHost)
			    hostName = InetAddress.getLocalHost().getHostName();
		}
	        else
		if ((char)ch == 'L') {
		      String virtualHost = go.optArgGet();
		      if (virtualHost.indexOf(".") < 0) {
			    hostName = virtualHost;
			    hostIP = InetAddress.getByName(
				      	  hostName).getHostAddress();
		      } else {
			    hostIP = virtualHost;
			    hostName = InetAddress.getByName(
					  hostIP).getHostName();
		      }
		      AMI_VirtualHost.setHostIP(hostIP);
		      isVirtualHost = true;
		}
    		else
		if ((char)ch == 'd') {
		      /* Obtain the distinguished name of user */
		      allProps = false;
		      userDNName = new String(go.optArgGet());
		}
	        else 
		if ((char)ch == 'e') {
		      /* Obtain the email of user */
		      allProps = false;
		      userDNSName = new String(go.optArgGet());
		} else { // print a usage clause (with keywords) 
		      System.err.println("\n" + messages.getString(
				    "AMI_Cmd.admin.usage1") + "\n");

		      System.err.println(messages.getString(
				    "AMI_Cmd.admin.usage2") + "\n");

		      for (int j = 0, k = 1; j < userPrefs.length;
			   j++, k++) {
			String x = new String(
					      userPrefs[j].visibleName);
			System.err.print("  " + x +
					 pad.substring(0, 23 - x.length()));
			if (k % 3 == 0)
			      System.err.println("");
		      }

		      System.err.println("\n\n" + messages.getString(
					 "AMI_Cmd.admin.usage3") + "  \n");
		      boolean nsKeyStoreProperty = false;
		      String sysProperty;
		      for (int j = 0, k = 1; j < sysProps.length; j++) {
			    sysProperty = sysProps[j].visibleName;
			    if (!sysProperty.startsWith("nsKeyStore") ||
				!nsKeyStoreProperty) {
			        if (sysProperty.startsWith(
						"nsKeyStore")) {
				       sysProperty = new String(
						     "nsKeyStore");
				       nsKeyStoreProperty = true;
				}
				String x = new String(sysProperty);
				System.err.print("  " + x +
						 pad.substring(0, 23 -
						 x.length()));
				if (k++ % 3 == 0)
				      System.err.println("");
			    }
		      }
		      
		      System.err.println("\n\n  " + messages.getString(
					 "AMI_Cmd.admin.usage4") + "\n");

		      throw new AMI_Exception();
		  }
		}

	      	for (int k = go.optIndexGet(); k < args.length; k++) {
			allProps = false;
		      	printList.addElement(args[k]);
	      	}

		/*
	 	* Check if the AMI object type is correctly set ie., host or
		* user flags
		*/
	      	if ((System.getProperty("user.name").equals("root")) &&
		    (hostType != AMI_Constants.AMI_HOST_OBJECT)) {
			System.err.println("\n" + messages.getString(
				"AMI_Cmd.admin.usehflag") + "\n");
			throw new AMI_Exception();
	      	}	      
	}

	/**
        *  This method reads the properties file and creates a hashtable.
        *  It also matches up the visible names in this class's local
        *  tables and when such matches are found they are associated
        *  in another hashtable to facilitate quick lookups and associations
        *  between the long fully-qualified property names and the
        *  visible names the user sees.
        *
        *  @parameter p  - The property file as input
        *
        */

	private static void build_hashtables(BufferedReader p) {
		String s;
		String propName;
		String propValue;

		/*
		*************************
		* Absorb properties file *
		*************************
		*/

		try {
			s = p.readLine();
		} catch (IOException e) {
			return;
		}

		while (s != null) {

			/* Truncate comments */
			int i = s.indexOf('#');
			if (i > -1)
				s = s.substring(0, i);

			/* Remove whitespace */
			s = s.trim();

			/* Load property entry if non-null */
			if (s.length() > 0) {

				propName = null;
				propValue = null;

				StringTokenizer st = new StringTokenizer(s);

				if (st.hasMoreTokens())
					propName = st.nextToken();

				if (st.hasMoreTokens()) {
					propValue = st.nextToken();
					if (propValue.indexOf("=") > -1)
						// Make sure we pick
						// up full DN
						while (st.hasMoreTokens())
							propValue =
							propValue +
						        " " + st.nextToken();
				}
				if (propName != null && propValue != null) {
					propHT.put(propName, propValue);
					/* Store property in hash table */

					/*
					 * Associate short cut tag in sysTagHT
					 * if matching entry found
					 */
				    for (int j = 0; j < sysProps.
				    length; j++) {
				        if (propName.indexOf(sysProps[j].
				        internalName) > -1) {
				            sysTagHT.put(sysProps[j].
					    visibleName, propName);
						break;
					}
				    }
				}
			}
			try {
				s = p.readLine();
			} catch (IOException e) {
				break;
			}
		}
	}


	/**
         *  This method goes through the worklist of system properties
         *  and user preferences assigned during the parsing phase,
         *  and actual updates the user preferences and properties
         *  file to the extent new values are assigned to either.
         *
         */

	private static void assign_values() throws Exception {
		BufferedWriter propfile = null;
		String propName;
		String fullPropName;
		String propValue;
		String previousPropValue = null;
		String theModifiedProp = null;
		String cachedVisibleName = null;
		Enumeration propList;
		int propCatagory;
		int propIndex = -1;
		boolean reWritePropFile = false;

		for (int k = 0; k < assignList.size(); k++) {

			propName = ((assignPair) assignList.get(k)).property;
			propValue = ((assignPair) assignList.get(k)).value;

			/* Determine catagory(pref or property) */
			propCatagory = SYSTEM_PROP;
			for (int j = 0; j < userPrefs.length; j++) {
				if ((userPrefs[j].visibleName).
				equals(propName)) {
					propCatagory = USER_PREF;
					propIndex = j;
					break;
				}
			}
			if (propCatagory == USER_PREF) {
				try {
					if (names == null)
						names = new AMINames();
				} catch (AMI_KeyMgntNotFoundException e) {
					System.err.println("\n" +
					    messages.getString(
					    "AMI_Cmd.admin.connect") + " \n");
					System.exit(-1);
				}

				switch (userPrefs[propIndex].methodIndex) {
				case DEFAULT_RSA_SIGNATURE_KEY:
					try {
						AMI_KeyMgntClient.
						setRSASignAlias(names,
						    names.userName,
						    propValue,
						    hostType);
					} catch (Exception e) {
						System.err.println("\n" +
						    messages.getString(
						    "AMI_Cmd.admin.connect") +
						    " \n");
						/* e.printStackTrace(); */
					}
					break;
				case DEFAULT_RSA_ENCRYPTION_KEY:
					try {
						AMI_KeyMgntClient.
						setRSAEncryptAlias(names,
						    names.userName,
						    propValue,
						    hostType);
					} catch (Exception e) {
						System.err.println("\n" +
						    messages.getString(
						    "AMI_Cmd.admin.connect") +
						    " \n");
						/* e.printStackTrace(); */
					}
					break;
				case DEFAULT_DSA_KEY:
					try {
						AMI_KeyMgntClient.setDSAAlias(
						    names,
						    names.userName,
						    propValue,
						    hostType);
					} catch (Exception e) {
						System.err.println("\n" +
						    messages.getString(
						    "AMI_Cmd.admin.connect") +
						    " \n");
						/* e.printStackTrace(); */
					}
					break;
				case DEFAULT_DH_KEY:
					try {
						AMI_KeyMgntClient.setDHAlias(
						    names,
						    names.userName,
						    propValue,
						    hostType);
					} catch (Exception e) {
						System.err.println("\n" +
						    messages.getString(
						    "AMI_Cmd.admin.connect") +
						    " \n");
						/* e.printStackTrace(); */
					}
					break;
				case KEYSTORE_FILE:
					try {
						AMI_KeyMgntClient.
						setKeyStoreFileName(names,
						    names.userName,
						    propValue,
						    hostType);
					} catch (Exception e) {
						System.err.println("\n" +
						    messages.getString(
						    "AMI_Cmd.admin.connect") +
						    " \n");
						/* e.printStackTrace(); */
					}
					break;
				case CERT_FILE:
					try {
						AMI_KeyMgntClient.
						setCertificateFileName(names,
						    names.userName,
						    propValue,
						    hostType);
					} catch (Exception e) {
						System.err.println("\n" +
						    messages.getString(
						    "AMI_Cmd.admin.connect") +
						    " \n");
						/* e.printStackTrace(); */
					}
					break;
				case CA_CERT_FILE:
					try {
						AMI_KeyMgntClient.
						setCaCertificateFileName(names,
						    names.userName,
						    propValue,
						    hostType);
					} catch (Exception e) {
						System.err.println("\n" +
						    messages.getString(
						    "AMI_Cmd.admin.connect") +
						    " \n");
						/* e.printStackTrace(); */
					}
					break;
				case CERT_CHAIN_FILE:
					try {
						AMI_KeyMgntClient.
						setCertificateChainFileName(
						names, names.userName,
						    propValue,
						    hostType);
					} catch (Exception e) {
						System.err.println("\n" +
						    messages.getString(
						    "AMI_Cmd.admin.connect") +
						    " \n");
						/* e.printStackTrace(); */
					}
					break;
				case CERT_REQ_FILE:
					try {
						AMI_KeyMgntClient.
						setCertReqFileName(names,
						    names.userName,
						    propValue,
						    hostType);
					} catch (Exception e) {
						System.err.println("\n" +
						    messages.getString(
						    "AMI_Cmd.admin.connect") +
						    " \n");
						/* e.printStackTrace(); */
					}
					break;
				case BACKUP_CERT_REQ_CERT_FILE:
					try {
						AMI_KeyMgntClient.
						setBackupCertFileName(names,
						    names.userName,
						    propValue,
						    hostType);
					} catch (Exception e) {
						System.err.println("\n" +
						    messages.getString(
						    "AMI_Cmd.admin.connect") +
						    " \n");
						/* e.printStackTrace(); */
					}
					break;
				case SET_NAME_DN:
					try {
						AMI_KeyMgntClient.
						setNameDNAlias(names,
						    names.userName,
						    propValue,
						    hostType);
					} catch (Exception e) {
						System.err.println("\n" +
						    messages.getString(
						    "AMI_Cmd.admin.connect") +
						    " \n");
						/* e.printStackTrace(); */
					}
					break;
				}

			} else {
				// System property
				int i;
				String s = null;

				/*
				* Look for property name in the list of
				* recognized visible names
				*/
				for (i = 0; i < sysProps.length; i++) {
					if ((sysProps[i].visibleName).equals(
					    propName)) {
						s = sysProps[i].internalName;
						break;
					}
				}
				/*
				* Abort with error if we don 't recognize it.
				*/
				if (i == sysProps.length) {
					try {
						Object[] params = {
							new String(propName)
						};
						System.err.println(
						msgFormatter.format(
						messages.getString(
						"AMI_Cmd.admin.unknownprop2"),
						params));
					} catch (Exception e) {
						throw new RuntimeException(
						    e.toString());
					}
					System.exit(-1);
				}
				/*
				* Translate visible property tag to
				* internal fully qualified property name,
				* printing error if no match.
				*/

				fullPropName = (String)sysTagHT.get(propName);

				if (fullPropName == null) {
					/*
					* force to a value that will trigger
					* the following error
					*/
					fullPropName = new 
					String("*NOT FOUND*");
				}

				// Make sure the full name is in our hashtable
				if (!propHT.containsKey(fullPropName)) {
					try {
						Object[] params = {
							new String(propName),
							 new String(s)
						};
						System.err.println(
						    msgFormatter.format(
						    messages.getString(
						    "AMI_Cmd.admin.unknownkey"),
						    params));
					} catch (Exception e) {
						throw new RuntimeException(
						    e.toString());
					}
					System.exit(-1);
					return;
				}
				/*
				 * Save the value we are about to modify so we
				 * can write the details to the AMI log file
				 * later
				 */
				cachedVisibleName = new String(propName);
				previousPropValue = (String) propHT.get(
				    fullPropName);
				theModifiedProp = new String(fullPropName);

				/*
				 * Overwrite the previous value with the one
				 * specified in command line.
				 */
				propHT.put(fullPropName, propValue);
				reWritePropFile = true;
			}

			if (reWritePropFile) {
				/* Time to rewrite it */
				if (!(System.getProperty(
				    "user.name").equals("root"))) {
					System.err.println(messages.getString(
					    "AMI_Cmd.admin.mustberoot"));
					System.exit(-1);
				}
				propfile = new BufferedWriter(
				    new FileWriter(PROPERTIES_FILE));
				propList = propHT.keys();
				while (propList.hasMoreElements()) {
					fullPropName = (String)
					    propList.nextElement();
					propValue = (String)
					    propHT.get(fullPropName);
					String x = fullPropName +
					    pad.substring(0, 45 -
					    fullPropName.length()) +
					    propValue;
					propfile.write(x, 0, x.length());

					/*
					 * If the current property was the
					 * modified one, log the details
					 */
					if (fullPropName.
					equals(theModifiedProp)) {
						/*
						 * Write an entry to the AMI
						 * logfile describing the AMI
						 * system property modified.
						 */

						try {
							Object[] params = {
							    new String(
							    cachedVisibleName),
							    new String(
							    fullPropName),
							    new String(
							    previousPropValue),
							    new 
							    String(propValue)
							};

							AMI_Log.writeLog(1,
							"AMI_Admin.modsysprop", 
							params);
						} catch (Exception e) {
							throw new 
							RuntimeException(
							e.toString());
						}
					}
					propfile.newLine();
				}
				propfile.close();
			}
		}
	}

	/**
         *  This method processes the worklist of user preferences
         *  and system properties to display.
         */
	private static void print_requested_props() {
		String reqProps;
		for (int k = 0; k < printList.size(); k++) {
			reqProps = (String) printList.get(k);
			if (reqProps.equals("nsKeyStore"))
				reqProps = new String("nsKeyStoreRSA");
			print_prop(reqProps, PRINT_HEADER);
		}
	}

	/**
         *  This method displays all system properties which can
         *  be modified by this program, and all user preferences
         *  recognized by this class.
         */
	private static void print_all_props() {
		Enumeration keyList;
		String visibleName;

		System.out.println("\n" + messages.getString(
		    "AMI_Cmd.admin.usage2") + " \n");
		for (int j = 0; j < userPrefs.length; j++)
			print_prop(userPrefs[j].visibleName, SUPPRESS_HEADER);

		System.out.println("\n" + messages.getString(
		    "AMI_Cmd.admin.usage3") + " \n");
		boolean keyStoreProperty = false;
		boolean nsProperty = false, prefixProperty = false,
		    ldapProperty = false, logProperty = false,
		    debugProperty = false, i18nProperty = false;
		String property;
		for (int i = 0; i < sysProps.length; i++) {
			property = sysProps[i].visibleName;
			if (property.startsWith("ns") && !nsProperty) {
				System.out.println("\n" + messages.getString(
				    "AMI_Cmd.admin.nsProperty") + " \n");
				nsProperty = true;
			}
			if (property.endsWith("Prefix") && !prefixProperty) {
				System.out.println("\n" + messages.getString(
				    "AMI_Cmd.admin.prefixProperty") + " \n");
				prefixProperty = true;
			}
			if (property.startsWith("ldap") && !ldapProperty) {
				System.out.println("\n" + messages.getString(
				    "AMI_Cmd.admin.ldapProperty") + " \n");
				ldapProperty = true;
			}
			if (property.startsWith("log") && !logProperty) {
				System.out.println("\n" + messages.getString(
				    "AMI_Cmd.admin.logProperty") + " \n");
				logProperty = true;
			}
			if (property.startsWith("debug") && !debugProperty) {
				System.out.println("\n" + messages.getString(
				    "AMI_Cmd.admin.debugProperty") + " \n");
				debugProperty = true;
			}
			if (property.startsWith("i18n") && !i18nProperty) {
				System.out.println("\n" + messages.getString(
				    "AMI_Cmd.admin.i18nProperty") + " \n");
				i18nProperty = true;
			}
			if (!property.equals("ldapPassword")) {
				if (property.startsWith("nsKeyStore")) {
					if (!keyStoreProperty) {
						print_prop(property,
						    SUPPRESS_HEADER);
						keyStoreProperty = true;
					}
				} else
					print_prop(property, SUPPRESS_HEADER);
			} else if (System.getProperty("user.name").
			        equals("root"))
				print_prop(property, SUPPRESS_HEADER);
		}
		System.out.println("");
	}

	/**
         *  This method displays a single property or preference.
         */
	private static void print_prop(String pReqProp, boolean printPropType) {

		int propCatagory = SYSTEM_PROP;
		/* Assume */
		int propIndex = -1;
		String propName;
		String propValue;
		String reqProp = new String(pReqProp);

		/* Determine if this is a user preference, instead */
		for (int j = 0; j < userPrefs.length; j++)
			if ((userPrefs[j].visibleName).equals(reqProp)) {
				propCatagory = USER_PREF;
				propIndex = j;
				break;
			}

		/* print the system property or user preference. */
		if (propCatagory == USER_PREF) {
			try {
				if (names == null)
					names = new AMINames();
			} catch (Exception e) {
				System.err.println("\n" +
				    messages.getString(
				    "AMI_Cmd.admin.connect") + " \n");
				/* System.exit(-1); */
			}

			if (reqProp.length() > 25)
				reqProp = reqProp.substring(0, 25);

			System.out.print("  "
			    + reqProp
			    + pad.substring(0, 25 - reqProp.length()) + " = ");

			switch (userPrefs[propIndex].methodIndex) {
			case DEFAULT_RSA_SIGNATURE_KEY:
				try {
					System.out.println(
					    AMI_KeyMgntClient.getRSASignAlias(
						names.userName,
						hostType));
				} catch (Exception e) {
					System.err.println("\n" +
					    messages.getString(
					    "AMI_Cmd.admin.connect") + " \n");
				}
				break;
			case DEFAULT_RSA_ENCRYPTION_KEY:
				try {
					System.out.println(
					    AMI_KeyMgntClient.
					    getRSAEncryptAlias(
					    names.userName,
					    hostType));
				} catch (Exception e) {
					System.err.println("\n" +
					    messages.getString(
					    "AMI_Cmd.admin.connect") + " \n");
				}
				break;
			case DEFAULT_DSA_KEY:
				try {
					System.out.println(
					    AMI_KeyMgntClient.getDSAAlias(
						names.userName,
						hostType));
				} catch (Exception e) {
					System.err.println("\n" +
					    messages.getString(
					    "AMI_Cmd.admin.connect") + " \n");
				}
				break;
			case DEFAULT_DH_KEY:
				try {
					System.out.println(
					    AMI_KeyMgntClient.getDHAlias(
						names.userName,
						hostType));
				} catch (Exception e) {
					System.err.println("\n" +
					    messages.getString(
					    "AMI_Cmd.admin.connect") + " \n");
				}
				break;
			case KEYSTORE_FILE:
				try {
					System.out.println(
					    AMI_KeyMgntClient.
					    getKeyStoreFileName(
						names.userName,
						hostType));
				} catch (Exception e) {
					System.err.println("\n" +
					    messages.getString(
					    "AMI_Cmd.admin.connect") + " \n");
				} break;
			case CERT_FILE:
				try {
					System.out.println(
			    AMI_KeyMgntClient.getCertificateFileName(
						names.userName,
						hostType));
				} catch (Exception e) {
					System.err.println("\n" +
					    messages.getString(
					    "AMI_Cmd.admin.connect") + " \n");
				} break;
			case CA_CERT_FILE:
				try {
					System.out.println(
			    AMI_KeyMgntClient.getCaCertificateFileName(
						names.userName,
						hostType));
				} catch (Exception e) {
					System.err.println("\n" +
					    messages.getString(
					    "AMI_Cmd.admin.connect") + " \n");
				} break;
			case CERT_CHAIN_FILE:
				try {
					System.out.println(
			    AMI_KeyMgntClient.getCertificateChainFileName(
						names.userName,
						hostType));
				} catch (Exception e) {
				 	System.err.println("\n" +
					    messages.getString(
					    "AMI_Cmd.admin.connect") + " \n");
				} break;
			case CERT_REQ_FILE:
				try {
					System.out.println(
					AMI_KeyMgntClient.
					getCertReqFileName(
					names.userName,
					hostType));
				} catch (Exception e) {
					System.err.println("\n" +
					    messages.getString(
					    "AMI_Cmd.admin.connect") + " \n");
				} break;
			case BACKUP_CERT_REQ_CERT_FILE:
				try {
					System.out.println(
					    AMI_KeyMgntClient.
					    getBackupCertFileName(
						names.userName,
						hostType));
				} catch (Exception e) {
					System.err.println("\n" +
					    messages.getString(
					    "AMI_Cmd.admin.connect") + " \n");
				} break;
			case SET_NAME_DN:
				try {
					String[] aliases =
					    AMI_KeyMgntClient.getNameDNAlias(
					    names.userName, null, hostType);
					if (aliases == null) {
						System.out.println(aliases);
						break;
					}
					for (int i = 0; i < aliases.length;
					i++) {
						if (i != 0)
							System.out.print("  "
							+ reqProp
							+ pad.substring(0,
							(25 - reqProp.length()))
							+ " = ");
						System.out.println(aliases[i]);
					}
				} catch (Exception e) {
					System.err.println("\n" +
					    messages.getString(
					    "AMI_Cmd.admin.connect") + " \n");
				} break;
			}
		} else {
			/* print System property */

			propName = (String) sysTagHT.get(reqProp);
			if (propName == null)
				propValue = messages.getString(
				    "AMI_Cmd.admin.unknownprop1");
			else
				propValue = (String) propHT.get(propName);

			/* We hide the keytype property names */
			if (reqProp.startsWith("nsKeyStore"))
				reqProp = new String("nsKeyStore");

			if (reqProp.length() > 25)
				reqProp = reqProp.substring(0, 25);

			System.out.println("  "
			    + reqProp
			    + pad.substring(0, 25 - reqProp.length())
			    + " = "
			    + propValue);
		}
	}

	/**
	 *
	 * Initialize internationalization.
	 *
	 */
	protected static void initialize() {
		if (initialized)
			return;
		initialized = true;

		try {
			Locale currentLocale = AMI_Constants.initLocale();
			msgFormatter = new MessageFormat("");
			msgFormatter.setLocale(currentLocale);
			messages = AMI_Constants.getMessageBundle(
			    currentLocale);
		} catch (Exception e) {
			System.err.print("Error in initialization of AMI");
			String msg = e.getMessage();
			if (msg != null)
				System.err.println(": " + msg);
			else
				System.err.println("");
			System.exit(-1);
			/* e.printStackTrace(); */
		}

		try {
			hostIP = InetAddress.getLocalHost().getHostAddress();
		} catch (UnknownHostException e) {
			System.err.println(
			    "\n" + messages.getString(
			    "AMI_Cmd.admin.unknownHost") + "\n");
			System.exit(-1);
		}


	}
}

/**
 *  This class associates a property name with its value
 */
class assignPair {
	String property;
	String value;
}

/**
 *  This class associates a visble property name with an internal
 *  (fully qualified) properly name.
 *
 */
class nameMap {
	String visibleName;
	String internalName;
	boolean backendSelection;


	public nameMap(String a, String b) {
		visibleName = a;
		internalName = b;
		backendSelection = false;
	}
	public nameMap(boolean f, String a, String b) {
		visibleName = a;
		internalName = b;
		backendSelection = f;
	}
}


/**
 *  This class associates a visible user preference with the
 *  case statement index to the method capable of displaying
 *  or modifying that preference.
 *
 */
class methodMap {
	String visibleName;
	int methodIndex;

	public methodMap(String a, int b) {
		visibleName = a;
		methodIndex = b;
	}
}
