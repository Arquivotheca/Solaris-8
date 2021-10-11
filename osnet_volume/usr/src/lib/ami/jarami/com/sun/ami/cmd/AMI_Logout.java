/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_Logout.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.cmd;

import java.lang.*;
import java.util.*;
import java.io.*;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.security.*;
import java.security.cert.*;
import java.security.cert.Certificate;
import java.security.KeyPair;
import java.security.spec.*;
import java.text.MessageFormat;
import java.math.BigInteger;

import com.sun.ami.AMI_Exception;
import com.sun.ami.common.AMI_Constants;
import com.sun.ami.common.AMI_Common;
import com.sun.ami.utils.AMI_C_Utils;
import com.sun.ami.utils.AMI_GetOpt;
import com.sun.ami.keymgnt.AMI_KeyMgntException;
import com.sun.ami.keymgnt.AMI_KeyMgntClient;

/**
 * This class provides a utility to logout from the servers. It un-registers
 * the key from the memory, and also removes it from permanent storage, if the
 * key was a permanent key.
 * The implementation classes need to provide the logoutFromServer method.
 *
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 */

public abstract class AMI_Logout extends AMI_Common {

    public AMI_Logout()  throws Exception {

           _props = System.getProperties();

	   // Get current users name and Id
	   _name = _props.getProperty("user.name");
	   _uids = new Vector();

	   // Set the locale
	   Locale currentLocale = AMI_Constants.initLocale();
           _msgFormatter = new MessageFormat("");
           _msgFormatter.setLocale(currentLocale);
           _messages = AMI_Constants.getMessageBundle(currentLocale);
    }

    protected void getOptions(String[] argv) 
       	throws AMI_UsageException, UnknownHostException,
	IOException, AMI_Exception
    {
            AMI_GetOpt go = new AMI_GetOpt(argv, "hL:");
	    int ch = -1;
	    boolean isVirtualHost = false;

	    while ((ch = go.getopt()) != go.optEOF) {

                /*
                 * options: -h < uids >
		 *          -L ipaddress or hostnames
                 */
	        if ((char)ch == 'h') {	 

		     if (!_name.equals("root")) {
		        throw new AMI_UsageException(_messages.
			getString("AMI_Cmd.logout.usage1"));
		     }
		     _hostLogin = true;
		     _permanent  |= AMI_Constants.AMI_PERMANENT;
		     if (!isVirtualHost) {
		         _name = InetAddress.getLocalHost().getHostName();
			 _hostIP = InetAddress.getLocalHost().getHostAddress();
		     }

		 }
		 else 
		 if ((char)ch == 'L') {	 
		    // Argument can be hostname or IP address.
		    String virtual = go.optArgGet();

		    // If HOSTNAME ::		    
		    if (virtual.indexOf(".") < 0) {		     
		        _name = virtual;
			_hostIP = InetAddress.getByName(_name).getHostAddress();

		    }		    
		    else {	 // If IP ::		    
		        _hostIP = virtual;

			// Get hostname for ip
			_name = InetAddress.getByName(_hostIP).getHostName();
		    }
		    isVirtualHost = true;
		 }
		 else
		   throw new AMI_UsageException("");
	    }

	    if (go.optIndexGet() < argv.length) {
	        if (_hostLogin)
		    for (int k = go.optIndexGet(); k < argv.length; k++) 
		    // This is valid only if host -h option 
		    // has been specified
		        _uids.addElement(argv[k]);		     
		 else 
		   throw new AMI_UsageException("");
	    }

	    if (System.getProperty("user.name").equals("root")
		&& (!_hostLogin))
	        throw new AMI_UsageException(_messages.
		getString("AMI_Cmd.logout.isrootuser"));    

	    if ((_hostIP != null) && (_permanent == 0))
	        throw new AMI_UsageException(_messages.
		getString("AMI_Cmd.logout.usage2"));
    }
     
    protected void usage() {
        System.err.println(_messages.getString("AMI_Cmd.logout.usage"));
    }

    protected abstract void logoutFromServer() throws Exception;

    protected boolean _hostLogin = false;
    protected String _hostIP = null;
    protected String _name = null;  // User or Host Name
    protected int    _permanent = 0;
    protected String _hostName = null;  // Host Name

    protected Vector _uids = null;
    protected Properties _props;
    
    protected ResourceBundle _messages;
    protected MessageFormat _msgFormatter;
}
