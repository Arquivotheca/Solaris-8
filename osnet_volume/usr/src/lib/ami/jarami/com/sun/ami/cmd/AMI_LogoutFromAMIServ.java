/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_LogoutFromAMIServ.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.cmd;

import java.lang.*;
import java.util.*;
import java.io.*;
import java.rmi.*;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.security.*;
import java.security.cert.*;
import java.security.cert.Certificate;
import java.security.KeyPair;
import java.security.spec.*;
import java.math.BigInteger;

import com.sun.ami.AMI_Exception;
import com.sun.ami.utils.AMI_C_Utils;
import com.sun.ami.amiserv.AMI_KeyServ;
import com.sun.ami.amiserv.AMI_KeyServClient;
import com.sun.ami.amiserv.AMI_EntryInfo;
import com.sun.ami.common.*;
import com.sun.ami.keymgnt.AMI_KeyMgntClient;

/**
 * This class extends the AMI_Logout, to provide a logout
 * mechanism from the AMI Server.
 *
 * 
 * @author Sangeeta Varma
 *
 * @version 1.0
 * @see AMI_Logout
 */

public class AMI_LogoutFromAMIServ extends AMI_Logout {

    public AMI_LogoutFromAMIServ() throws Exception {

           super();	   
	   AMI_C_Utils utils = new AMI_C_Utils();

	   _userId = utils.ami_get_user_id();
           _entries = new Vector();
    }

    public static void main(String[] args)  {
       
            AMI_LogoutFromAMIServ logout = null;
	    try {
               logout = new AMI_LogoutFromAMIServ();
	    } catch (Exception e) {
	        System.out.println("AMI_LogoutFromAMIServ:: " + e.toString());
	 	e.printStackTrace();		 
	        return;
	    }

	    try {
	        logout.getOptions(args);
	    } catch (AMI_UsageException e) {
	        System.out.println(e.toString());
	        logout.usage();
	        return;
	    } catch (Exception e) {
	        System.out.println("AMI_LogoutFromAMIServ:: " + e.toString());
	 	e.printStackTrace();		 
	        return;
	    }

	    try {
	        logout.logoutFromServer();
	    } catch (Exception e) {
	        System.out.println("AMI_LogoutFromAMIServ:: " + e.toString());
	 	e.printStackTrace();
	        return;		 
	    }	    
    }

    protected void logoutFromServer() throws Exception {

          AMI_KeyServ client = AMI_KeyServClient.getInstance(
	    AMI_KeyMgntClient.getProperty(AMI_Constants.PROTOCOL_PROPERTY));

	  if (_hostLogin) {
	       	if (_uids.size() != 0) {
		    for (int ii = 0; ii < _uids.size(); ii++)
		        _entries.addElement(new AMI_EntryInfo(_name,
			_hostIP, Long.parseLong((String)_uids.
			elementAt(ii)), null));

	       	}
	       	else 
		   _entries.addElement(new AMI_EntryInfo(_name,
			_hostIP, 0, "root"));
	  } else {
	       	_entries.addElement(new AMI_EntryInfo(InetAddress.
		getLocalHost().getHostName(), InetAddress.
		getLocalHost().getHostAddress(), _userId, _name));
	  }

	  // Remove the keystores from the server
	  try {
		AMI_Debug.debugln(3, "Removing KeyStores "); 

	        client.setKeyStore(null, null, _entries, _permanent,
		null, null, null);

		 System.out.println(_messages.
		getString("AMI_Cmd.logout.success"));
	  } catch (Exception e) {
	       	throw new Exception(_messages.
		getString("AMI_Cmd.logout.deregister" + e.toString()));
	  }
	  
    }

    private long _userId;
    private Vector _entries;

}
