/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_KeyServClient.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.amiserv;

import java.util.Vector;

import java.io.IOException;
import com.sun.ami.AMI_Exception;
import com.sun.ami.common.AMI_Debug;

/** 
 * 
 *
 * @author     Sangeeta Varma
 */

public class AMI_KeyServClient {

	public static AMI_KeyServ getInstance(String protocol)
	    throws AMI_Exception, IOException {

		AMI_KeyServ answer = null;

		AMI_Debug.debugln(3, "In AMI_KeyServClient::protocol = " +
			    protocol);

	       	if (!protocol.equalsIgnoreCase("RPC") &&
		    !protocol.equalsIgnoreCase("RMI"))
			return (null);

		String className = new String(
		    "com.sun.ami.amiserv.AMI_KeyServClient_" +
		    protocol.toUpperCase());
		
		try {
			Class cl = Class.forName(className);
			answer = (AMI_KeyServ) cl.newInstance();
			AMI_Debug.debugln(3, "AMI_KeyServClient:: " +
					  "Found Instance for class");

		} catch (ClassNotFoundException e) {
			AMI_Debug.debugln(1, "ClassNotFoundException " +
			    className);
			throw new AMI_Exception(e.getMessage());
		} catch (InstantiationException f) {
			AMI_Debug.debugln(1, "InstantiationException " +
			    className);
			throw new AMI_Exception(f.getMessage());
		} catch (IllegalAccessException g) {
			AMI_Debug.debugln(1, "IllegalAccessException " +
			    className);
			throw new AMI_Exception(g.getMessage());
		} catch (SecurityException h) {
			AMI_Debug.debugln(1, "SecurityException " +
			    className);
			throw new AMI_Exception(h.getMessage());
		}
		return (answer);
	}
}
