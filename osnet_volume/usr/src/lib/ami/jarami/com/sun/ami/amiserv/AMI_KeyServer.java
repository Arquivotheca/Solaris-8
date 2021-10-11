/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_KeyServer.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.amiserv;

import java.io.IOException;
import com.sun.ami.AMI_Exception;
import com.sun.ami.common.AMI_Debug;

public class AMI_KeyServer {

	public static AMI_KeyServ getInstance(String protocol)
	    throws AMI_Exception, IOException {

		AMI_KeyServ answer = null;

		if (!protocol.equalsIgnoreCase("RPC") ||
		    !protocol.equalsIgnoreCase("RMI"))
			return (null);

		String className = new String(
		    "com.sun.ami.amiserv.AMI_KeyServer_" +
		    protocol.toUpperCase());
		try {
			Class cl = Class.forName(className);
			answer = (AMI_KeyServ) cl.newInstance();
		} catch (ClassNotFoundException e) {
			AMI_Debug.debugln(1, "ClassNotFoundException " +
			    className);
		} catch (InstantiationException f) {
			AMI_Debug.debugln(1, "InstantiationException " +
			    className);
		} catch (IllegalAccessException g) {
			AMI_Debug.debugln(1, "IllegalAccessException " +
			    className);
		} catch (SecurityException h) {
			AMI_Debug.debugln(1, "SecurityException " +
			    className);
		}
		return (answer);
	}
}
