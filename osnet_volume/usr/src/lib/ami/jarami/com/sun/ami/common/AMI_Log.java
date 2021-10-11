/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_Log.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.common;

import java.util.*;
import java.io.*;

import com.sun.ami.keymgnt.AMI_KeyMgntException;

/**
 * This class provides logging capability for AMI. The level of logging 
 * required can be specified in the ami.properties file. 
 * By default, the log will be output to the "/tmp/ami_logfile" file. 
 * If a different file is to be used, it should be specified in the 
 * ami.properties file. The user specifies the data to be output to the 
 * log. In addition to that, the date
 * and time and the name of the user carrying out the operation, is logged.
 * This class logs data in a locale-independent fashion. The locale is also 
 * read from the ami.properties file. 
 * 
 * @version     1.0  
 * @author     Sangeeta Varma
 * 
 * @see AMI_AuditTrace
 */

public class AMI_Log extends AMI_AuditTrace
{
	private static String AMI_LOG_FILENAME = "/tmp/ami_logfile";
	private static String AMI_LOG_LEVEL = "ami.logging.level";
	private static String AMI_LOG_FILE = "ami.logging.filename";
	private static String AMI_LOG_PROPERTIES = "AMI_Messages_Log";

	public AMI_Log() {
	}

    /*  
     * Read the properties file for the level of logging 
     * and the log file name 
     */
	protected static void init() throws IOException, AMI_KeyMgntException,
	Exception  {

		// Read the properties file for the log level
		_logLevel = getLevel(AMI_LOG_LEVEL);

		// If log level is set, open the log file 
		if (_logLevel > 0) {		  
		  _os = openStream(AMI_LOG_FILE, AMI_LOG_FILENAME);
		  initLocale();
		}	       	  	     	     
		_props = System.getProperties();
		_initialised = true;
	}
  
    /*
     * Writes the data to the default log file.
     * @param level The level of logging for which this data 
     * should be output to the file.
     * @param key  The key to the data in the messages properties file.
     * @throws AMI_KeyMgntException If unable to read the properties file
     * @throws IOException If unable to write to log file 
     */
	public static void writeLog(int level, String key) 
				throws IOException, AMI_KeyMgntException,
				Exception  {

		// If initialisation has not been done, do it now.
		if (!_initialised) 
			init();	       

		// Check if current level of logging as per properties file, 
		// requires this data 
		// to be logged
		if (isOn(_logLevel, level))
			_os.println(_dateFormatter.format(new Date())+ "::" + 
			_messages.getString("AMI_Log.username") + " " + 
			_props.getProperty("user.name") + "::"  +  
			_messages.getString(key));
	}


    /*
     * Writes the data to the default log file, with appropriate variables 
     * filled in.
     * @param level The level of logging for which this data should
     * be output to the file.
     * @param key  The key to the data in the messages properties file.
     * @param arguments An array of objects containing the variables
     * that need to populate the data being logged.
     * @throws AMI_KeyMgntException If unable to read the properties file
     * @throws IOException If unable to write to log file 
     */
	public static void writeLog(int level, String key, Object[] arguments) 
				  throws IOException, AMI_KeyMgntException ,
				  Exception 
	{

		// If initialisation has not been done, do it now.
		if (!_initialised) 
			init();	       

		// Check if current level of logging as per properties file, 
		// requires this data to be logged	
		// If so, output the log data

		if (isOn(_logLevel, level)) {
			_msgFormatter.applyPattern(_messages.getString(key));
			_os.println(_dateFormatter.format(new Date())+ "::" +
			_messages.getString("AMI_Log.username")  + " " +
			_props.getProperty("user.name") + "::"  +  
			_msgFormatter.format(arguments));
		}
	}

	private static int _logLevel = 0;
	private static boolean _initialised = false;
	private static PrintStream _os = null;
	private static Properties _props = null;
}
