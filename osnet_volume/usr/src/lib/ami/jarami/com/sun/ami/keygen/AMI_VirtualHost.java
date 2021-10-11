/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_VirtualHost.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.keygen;

import com.sun.ami.utils.AMI_C_Utils;
import com.sun.ami.AMI_Exception;
import java.net.InetAddress;

/**
 * 
 * A class conatining static methods, to enable Virtual hosting. 
 * 
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 */
public class AMI_VirtualHost {

    public AMI_VirtualHost() {}

    public static void setHostIP(String ip) throws AMI_Exception {

        AMI_C_Utils utils = new AMI_C_Utils();
	if (utils.ami_get_user_id() != 0)
	    throw new AMI_Exception(
		"AMI_VirtualHost:: Only a root user can set Host IP");

	_hostIP = ip;
    }

    public static String getHostIP() {
	if (_hostIP == null) {
		try {
			InetAddress localhost = InetAddress.getLocalHost();
			return (localhost.getHostAddress());
		} catch (java.net.UnknownHostException e) {
			return (null);
		}
	}
        return _hostIP;
    }

    protected static String _hostIP = null;
}
