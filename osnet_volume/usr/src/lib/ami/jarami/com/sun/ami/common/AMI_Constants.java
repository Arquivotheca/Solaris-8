/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_Constants.java	1.1 99/07/11 SMI"
 *
 */


package com.sun.ami.common;

import java.io.IOException;
import java.util.*;
import java.net.*;

import com.sun.ami.keymgnt.AMI_KeyMgntClient;
import com.sun.ami.keymgnt.AMI_KeyMgntException;
/**
 * This class provides constants for AMI.
 *
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 */

public class AMI_Constants {

	public static final int AMI_PERMANENT = 1;
	public static final int AMI_USER_LOGIN = 3;
	public static final int AMI_HOST_LOGIN = 5;

	public static final String AMI_PERM_KEYS_FILENAME = 
					"/etc/ami/server/keys";
	public static final String AMI_PERM_KEYS_DIR = "/etc/ami/server";

	public static final String DEFAULT_ALIAS = "mykey";

	public static final String AMI_USER_OBJECT = "user";
	public static final String AMI_HOST_OBJECT = "host";
	public static final String AMI_APPLICATION_OBJECT = 
					"application";

	public static final String DEFAULT_RSA_ALIAS = "mykeyrsa";
	public static final String DEFAULT_DSA_ALIAS = "mykeydsa";
	public static final String DEFAULT_DH_ALIAS = "mykeydh";

	public static final int SIGN = 0;
	public static final int ENCRYPT = 1;

	public static final String DEFAULT_KEYSTORE_NAME = ".keystore";

	public static String LANGUAGE = "ami.locale.language";
	public static String COUNTRY = "ami.locale.country";
	public static String ENGLISH = "en";
	public static String USA = "US";

	public static String PROTOCOL_PROPERTY = "ami.comm.protocol";

	public static int AMI_MAX_DH_WEAK_KEYSIZE = 512;
	public static int AMI_MAX_DH_GLOBAL_KEYSIZE = 1024;
	public static int AMI_MAX_DH_DOMESTIC_KEYSIZE = 4096;
	private static final String BASE_BUNDLE_NAME =
	"com/sun/ami/AMI_Messages";

	public AMI_Constants() {

	}

	public static Locale initLocale()  
	    throws IOException, AMI_KeyMgntException {
		String language = null;
		String country = null;

		// Read the locale settings
		if ((language = 
			AMI_KeyMgntClient.getProperty(LANGUAGE)) == null)
		       language = ENGLISH;

		if ((country = 
			AMI_KeyMgntClient.getProperty(COUNTRY)) == null)
		       country = USA;

		// Set the messages bundle, and formatter classes.
		Locale currentLocale = new Locale(language, country);
		return currentLocale;
	}

	public static ResourceBundle getMessageBundle(Locale locale)
	    throws Exception {
		ResourceBundle msgBundle = null;

	   	// First try the Solaris Java locale area
	    	try {
			URL[] urls = new URL[] {
			    new URL("file:/usr/share/lib/locale/")
			};
			URLClassLoader ld = new URLClassLoader(urls);

			msgBundle = ResourceBundle.getBundle(
			    BASE_BUNDLE_NAME, locale, ld);
		} catch (MalformedURLException e) {
			// shouldn't get here
		} catch (MissingResourceException ex) {
			// Try the default one in the package ie., break
		}

		// Default resouce bundle
		try {
			msgBundle = ResourceBundle.getBundle(
			    BASE_BUNDLE_NAME, locale);
		} catch (MissingResourceException f) {
			System.err.println("Missing resource bundle ``"+
			    "/usr/share/lib/locale/" + BASE_BUNDLE_NAME +
			    "'' for locale ``" +
			    locale + "''; trying default...");
			throw f;
		}

		return msgBundle;
	}
}
