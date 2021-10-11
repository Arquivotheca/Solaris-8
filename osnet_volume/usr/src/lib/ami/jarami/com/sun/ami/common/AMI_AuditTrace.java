/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_AuditTrace.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.common;

import java.util.*;
import java.net.*;
import java.io.*;
import java.text.MessageFormat;
import java.text.DateFormat;

import com.sun.ami.keymgnt.AMI_KeyMgntClient;
import com.sun.ami.keymgnt.AMI_KeyMgntException;

/**
 * Any auditing feature for AMI should extend this class. This provides base 
 * features for auditing. 
 * Currently AMI_Log and AMI_Debug extend this to provide logging and debugging
 * functionality.
 *
 * @version     1.0  
 * @author     Sangeeta Varma
 */

public class AMI_AuditTrace
{
	private static final String LOG_BASE_BUNDLE_NAME =
	"com/sun/ami/AMI_Messages_Log";

	public AMI_AuditTrace() {
	}

	protected static int getLevel(String propertyName)
		 throws AMI_KeyMgntException, IOException 
	{
		return Integer.parseInt(
		AMI_KeyMgntClient.getProperty(propertyName));
	}

	protected static PrintStream openStream(String propertyName, 
					Object file) 
				      throws IOException, AMI_KeyMgntException
	{
		String filename = null;
		PrintStream ps = null;

		// Read the filename from properties file. If it is present, 
		// open that
		// file, otherwise, open the file specified by the second param.
		if ((filename = AMI_KeyMgntClient.getProperty(propertyName)) 
		    == null) {
	             if (file instanceof String) {
		          File fileObject = new File((String)file);
			
			  if (fileObject.exists() && !(fileObject.canWrite())) {
				System.out.println(
					"Cannot access debug log file: '"+
					(String)file+"'.  Using /dev/null");
		                ps = new PrintStream(new FileOutputStream(
							"/dev/null", true));
				return ps;
			  }
		          ps = new PrintStream(
				new FileOutputStream((String)file, true));
		     }
		     else
		          ps =  new PrintStream((OutputStream)file);
		} else  {
		    File fileObject = new File(filename);
		    if (fileObject.exists() && !(fileObject.canWrite())) {
			System.out.println("Cannot access debug log file: '"+
				(String)filename+"'.  Using /dev/null");
		        ps = new PrintStream(
				new FileOutputStream("/dev/null", true));
			return ps;
		    }
			ps = new PrintStream(
				new FileOutputStream(filename, true));
		}

		return ps;
	}

	protected static void initLocale() 
			throws IOException, AMI_KeyMgntException, Exception
	{

		String language = null;
		String country = null;

		// Read the locale settings 
		if ((language = 
		AMI_KeyMgntClient.getProperty(AMI_Constants.LANGUAGE)) == null)
		       language = AMI_Constants.ENGLISH;

		if ((country = AMI_KeyMgntClient.getProperty(
		    AMI_Constants.COUNTRY)) == null)
			country = AMI_Constants.USA;

		// Set the messages bundle, and formatter classes.
		Locale currentLocale = new Locale(language, country);
		_dateFormatter = DateFormat.getDateTimeInstance(
				DateFormat.SHORT, DateFormat.SHORT, 
				currentLocale);

		_msgFormatter = new MessageFormat("");
		_msgFormatter.setLocale(currentLocale);

		_messages = getMessageBundle(currentLocale);
	}
  
	public static ResourceBundle getMessageBundle(Locale locale) throws
	Exception {

	ResourceBundle msgBundle = null;

   	//First try the Solaris Java locale area

    	try {
	    URL[] urls = new URL[] { new URL("file:/usr/share/lib/locale/") };

	    URLClassLoader ld = new URLClassLoader(urls);

	    msgBundle = ResourceBundle.getBundle(LOG_BASE_BUNDLE_NAME, locale, ld);

	    return msgBundle;
        } catch (MalformedURLException e) {	// shouldn't get here
        } catch (MissingResourceException ex) {
	    System.err.println("Missing resource bundle ``"+
			   "/usr/share/lib/locale/" + LOG_BASE_BUNDLE_NAME +
			   "'' for locale ``" +
			   locale + "''; trying default...");
	    throw ex;
        }

        try {
	    msgBundle = ResourceBundle.getBundle(LOG_BASE_BUNDLE_NAME,locale);
    	} catch( MissingResourceException ex ) {   //can't localize this one!

	//We can't print out to the log, because we may be in the
	//  process of trying to.

	    System.err.println("Missing resource bundle ``"+
		LOG_BASE_BUNDLE_NAME+
		 "'' for locale ``"+
		 locale+
		 "''");
	    throw ex;
	}

	return msgBundle;
    }
	protected static boolean isOn(int propLevel, int level) {

		if ((propLevel == 0) || (propLevel < level))
		    return false;
		else
		    return true;
	}

	protected static ResourceBundle _messages;
	protected static DateFormat _dateFormatter;
	protected static MessageFormat _msgFormatter;
}
