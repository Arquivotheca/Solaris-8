/*
 *
 * ident	"@(#)pmUtility.java	1.2	99/03/29 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmUtility.java
 * Resource loading and utility classes
 */

package com.sun.admin.pm.client;

import java.awt.*;
import java.applet.*;
import java.io.*;
import java.util.*;
import javax.swing.*;

import com.sun.admin.pm.server.*;


/*
 * Utility class to provide common functions to the printing
 * manager classes
 */

public class pmUtility {

/*
 * Gets the localized string from the named bundle
 */

    public static String getResource(String key) {
	String keyvalue = null;
	ResourceBundle bundle = null;


	try {
		bundle = ResourceBundle.getBundle(
			"com.sun.admin.pm.client.pmResources");
	} catch (MissingResourceException e) {
		Debug.fatal("Could not load pmResources file");
	}

	try {
		keyvalue = bundle.getString(key);
	} catch (MissingResourceException e) {
		Debug.error("CLNT: getResource: Missing: " + key);
           keyvalue = new String("<<" + key + ">>");
	}

	return keyvalue;
    }

    public static int getIntResource(String key) {
	int keyvalue = 0;
	String s = null;
	ResourceBundle bundle = null;
	
	try {
		bundle = ResourceBundle.getBundle(
			"com.sun.admin.pm.client.pmResources");
	} catch (MissingResourceException e) {
		Debug.fatal("Could not load pmResources file");
	}
	
	try {
    		s = bundle.getString(key);
	} catch (MissingResourceException e) {
		Debug.error("Missing: " + key);
	}
	
	Debug.message("Resource: " + key + " Value: " + s);
	
	if (s != null) {
		try {
		    keyvalue = s.charAt(0);
            	} catch (Exception x) {
		    Debug.error("Resource: " + key + " threw: " + x);
		}
        }
 
	return keyvalue;
    }

    public static void doLogin(
	pmTop mytop, JFrame frame) throws pmGuiException {

	pmLogin l;

	if (mytop.ns.getNameService().equals("nis")) {

	    l = new pmLogin(
		frame,
		pmUtility.getResource("NIS.Authentication"),
		pmUtility.getResource("Enter.NIS.authentication.data."),
		mytop,
		"NISAuthentication");

	    l.show();

	    if ((l.getValue() != JOptionPane.OK_OPTION) &&
		 (l.getValue() != JOptionPane.CANCEL_OPTION)) {

	        	pmMessageDialog m = new pmMessageDialog(
                           	frame,
                            	pmUtility.getResource("Login.Failure"),
                            	pmUtility.getResource(
					"Request.cannot.be.completed."));
	        	m.show();
	        	throw new pmGuiException
                    		("pmAccess: Cannot create Login screen");
	    }

	    if (l.getValue() == JOptionPane.CANCEL_OPTION) {
		    throw new pmUserCancelledException("User.Cancelled.Login");
	    } else {

		// Pass data to backend

		    // getPassword sends back untrimmed string that is invalid
		    // as a password as it's too long
		    String tmpp = new String(l.passwordField.getPassword());
		    mytop.ns.setPasswd(tmpp.trim());

		    try {
		    	mytop.ns.checkAuth();
				Debug.message("doLogin():checkauth() OK");
		    } catch (Exception e) { 
			Debug.warning("doLogin:checkAuth()exception " + e);
			throw new pmGuiException("Login.Authorization.Failed");
		    }
	    }

	// User has not put in printer or server
	} else {
    	    pmMessageDialog m =
		new pmMessageDialog(
			frame,
			pmUtility.getResource("Login.Failure"),
			pmUtility.getResource("Request.cannot.be.completed."),
			mytop, "LoginFailed");

	    m.show();
	    throw new pmGuiException("pmAccess: Cannot create Login screen");
	}
        
    }


}
