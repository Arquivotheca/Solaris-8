/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_Debug.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.common;

import java.util.*;
import java.io.*;

import com.sun.ami.keymgnt.AMI_KeyMgntException;

/**
 *
 * This class provides debugging capability for AMI. The level of 
 * debugging required can be specified in the ami.properties file. 
 * By default, the debug output will be sent to the screen. If the debug 
 * output is to be stored in a file, it should be specified in the 
 * ami.properties file.
 * 
 * @version     1.0  
 * @author     Sangeeta Varma
 *
 * @see AMI_AuditTrace
 */
public class AMI_Debug extends AMI_AuditTrace
{
	private static String AMI_DEBUG_LEVEL = "ami.debugging.level";
	private static String AMI_DEBUG_FILE = "ami.debugging.filename";

	public AMI_Debug() {
	}

	/* Read the properties file and initialise the output stream */
	protected static void init() 
		throws IOException, AMI_KeyMgntException {

		// Read the properties file for the log level
		_debugLevel = getLevel(AMI_DEBUG_LEVEL);

		// If debug level is set, open the log file 
		// (default is system.out) 
		if (_debugLevel > 0) {
		  _os = openStream(AMI_DEBUG_FILE, System.out);
		}
		_initialised = true;
	}

	/*
	* Write the debug data, terminated with a "\n";
	* @param level The level of debugging for which this data should 
	* be output
	* @param data The debug data
	* @throws AMI_KeyMgntException If unable to get data from properties 
	* file.
	* @throws Exception If unable to open/write file.
	*/
	public static void debugln(int level, String data) 
				 throws AMI_KeyMgntException, IOException
	{
		if (!_initialised) 
			init();	       

		if (isOn(_debugLevel, level))
			_os.println(data);
	}

	/*
	* Write the debug data, without a terminating "\n"
	* @param level The level of debugging for which this data should 
	* be output
	* @param data The debug data
	* @throws AMI_KeyMgntException If unable to get data from properties 
	* file.
	* @throws Exception If unable to open/write file.
	*/
	public static void debug(int level, String data)
				throws AMI_KeyMgntException, IOException
	{
		if (!_initialised) 
			init();	       

		if (isOn(_debugLevel, level))
			_os.print(data);
	}

	/*
	* Write out each element of the array(without "\n"'s)
	* @param level The level of debugging for which this data should 
	* be output
	* @param array The array of objects to be written out
	* @throws AMI_KeyMgntException If unable to get data from properties 
	* file.
	* @throws Exception If unable to open/write file.
	*/
	public static void debugArray(int level, Object[] array) 
				throws AMI_KeyMgntException, IOException
	{

		if (!_initialised) 
			init();	       

		if (isOn(_debugLevel, level)) {
			for (int ii = 0; ii < array.length; ii++)
			   _os.print(array[ii] + " ");
			_os.println("");
		}
	}

	/*
	* Write out each element of the array(seperated by "\n")
	* @param level The level of debugging for which this data should 
	* be output
	* @param array The array of objects to be written out
	* @throws AMI_KeyMgntException If unable to get data from properties 
	* file.
	* @throws Exception If unable to open/write file.
	*/
	public static void debugArrayln(int level, Object[] array) 
				throws AMI_KeyMgntException, IOException
	{
		if (!_initialised) 
			init();	       

		if (isOn(_debugLevel, level)) {
			for (int ii = 0; ii < array.length; ii++)
			  _os.println("Array [" + ii + "] : " + array[ii]);
		}
	}

	private static int _debugLevel = 0;
	private static boolean _initialised = false;
	private static PrintStream _os = null;

}
