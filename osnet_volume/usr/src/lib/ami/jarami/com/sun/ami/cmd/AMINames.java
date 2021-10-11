/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMINames.java	1.1 99/07/11 SMI"
 *
 */

/**
 *  This method implements an abstract class.
 *
 *  AMI_KeyMgntService is an interface class declaring methods
 *  getNameLogin(), getNameDN(), and getNameDNS().  In order to use
 *  AMI_KeyMgntClient set...Alias() methods to alter user preferences,
 *  we must implement AMI_KeyMgntService, which is passed to the
 *  set...Alias() methods as the first parameter.  To be able to
 *  provide such an object, we must define these interface methods.
 *
 */

package com.sun.ami.cmd;

import java.net.InetAddress;
import java.net.UnknownHostException;
import java.io.*;
import java.util.*;
import sun.security.x509.*;
import com.sun.ami.AMI_Exception;
import com.sun.ami.keymgnt.AMI_KeyMgntService;
import com.sun.ami.keymgnt.AMI_KeyMgntClient;
import com.sun.ami.keygen.AMI_VirtualHost;

public class AMINames implements AMI_KeyMgntService {

	public static String userName = null;
	public static String userDNName = null;
	public static String userDNSName = null;

	public AMINames() throws Exception {
		// Since this class depends on AMI Admin
		AMI_Admin.initialize();

		userName = System.getProperty("user.name");
		if (userName.equals("root")) {
			userName = AMI_VirtualHost.getHostIP();
			if (userName == null) {
				System.err.println("\n" +
				    AMI_Admin.messages.getString(
				    "AMI_Cmd.admin.unknownHost") + "\n");
				System.exit(1);
			}
		}
		if (AMI_Admin.userDNName != null)
			userDNName = new String(AMI_Admin.userDNName);
		else {
			userDNName =
			    AMI_KeyMgntClient.getDNNameFromLoginName(
			    userName, AMI_Admin.hostType);
		}

		if (AMI_Admin.userDNSName != null)
			userDNSName = new String(AMI_Admin.userDNSName);
		else {
			userDNSName =
			    AMI_KeyMgntClient.getDNSNameFromLoginName(
			    userName, AMI_Admin.hostType);
		}
	}

	public String getNameLogin() {
		return userName;
	}

	public String getNameDN() {
		if (userDNName == null) {
			/* Input reader */
			BufferedReader reader =
			new BufferedReader(
			    new InputStreamReader(System.in));

			/* Get the DN Name from the user */
			System.out.println(
			    "\n" + AMI_Admin.messages.getString(
			    "AMI_Cmd.admin.promptdn"));
			try {
				userDNName = reader.readLine();
			} catch (IOException e) {
				e.printStackTrace();
			}
		}
		if (!validateDN(userDNName)) {
			System.out.println(
			    "\n" + AMI_Admin.messages.getString(
			    "AMI_Cmd.admin.invalidDN"));
			System.exit(1);
		}
		try {
			X500Name x = new X500Name(userDNName);
		} catch (Exception e) {
			System.err.println(
			    "\n" + AMI_Admin.messages.getString(
			    "AMI_Cmd.admin.invalidDN"));
			System.exit(1);
		}

		return userDNName;
	}

	public String getNameDNS() {
		if (userDNSName == null) {
			/* Input reader */
			BufferedReader reader = new BufferedReader(
			    new InputStreamReader(System.in));

			/* Get the DN Name from the user */
			System.err.println(
			    "\n" + AMI_Admin.messages.getString(
			    "AMI_Cmd.admin.promptemail"));
			try {
				userDNSName = reader.readLine();
				if (userDNSName == null)
					userDNSName = "";
			} catch (IOException e) {
				e.printStackTrace();
			}
		}
		return userDNSName;
	}

	boolean validateDN(String theDN) {
		boolean validDN = false;
		StringTokenizer a = new StringTokenizer(theDN, ","), b;
		String dnParts[] = {"cn", "c", "l", "o", "ou", "s",
		"street", "t"};

		while (a.hasMoreTokens()) {
			validDN = true;
			b = new StringTokenizer(a.nextToken(), "=");
			if (!b.hasMoreTokens())
				return (false);
			String s1 = b.nextToken().toLowerCase().trim();
			if (!b.hasMoreTokens())
				 return (false);
			String s2 = b.nextToken().toLowerCase().trim();

			/* Check for too many '=' */
			if (b.hasMoreTokens())
				 return (false);
			boolean validPart = false;
			for (int i = 0; i < dnParts.length; i++) {
				if (s1.equals(dnParts[i])) {
					validPart = true;
					break;
				}
			}
			if (!validPart)
				return (false);

			/*
			 * Now check other token for printable. Don't worry
			 * about punctuation, it may be a valid part of the
			 * names given,
			 * 
			 * For example: Smith, Smith, Barney,
			 * Barney & Sons, Inc.
			 * 
			 * Which may have been specfied as
			 * 
			 * O=Smith\, Smith\, Barney\, Barney \& Sons\, Inc\.
			 * 
			 */

			for (int i = 0; i < s2.length(); i++) {
				if (s2.charAt(i) < 32 ||
				    s2.charAt(i) > 126) {
					return (false);
				}
			}

		}
		return (validDN);
	}

}
