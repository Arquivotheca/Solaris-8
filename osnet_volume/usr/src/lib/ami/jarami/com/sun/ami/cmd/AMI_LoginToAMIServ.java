/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_LoginToAMIServ.java	1.1 99/07/11 SMI"
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
import sun.misc.BASE64Decoder;

import com.sun.ami.AMI_Exception;
import com.sun.ami.common.*;
import com.sun.ami.utils.AMI_C_Utils;
import com.sun.ami.keymgnt.AMI_KeyMgntClient;
import com.sun.ami.amiserv.AMI_KeyServ;
import com.sun.ami.amiserv.AMI_KeyServClient;
import com.sun.ami.amiserv.AMI_EntryInfo;

/**
 * This class provides a command by which a user can register their
 * keystore  with the AMI server.
 * The keystore can reside in any naming service or in a file ( in the users
 * home directory).
 * On running this command, the user is prompted for the keystore password,
 * which is used for accessing the keystore.
 * This command can be run for users or for hosts ( with the -h option). The 
 * The -h option can be used only by the root user. If a login needs to be done
 * for on behalf of another user (or process id), the uids can be specified with
 * the -h option ( but only by root ). 
 *
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 */

public class AMI_LoginToAMIServ extends AMI_Login {


	public AMI_LoginToAMIServ() throws Exception {

            super();	   
           _entries = new Vector();
      	}

      	public static void main(String[] args)  {
       
            AMI_LoginToAMIServ login = null;

	    try {
	        login = new AMI_LoginToAMIServ();
	    } catch (Exception e) {
	        System.out.println("AMI_LoginToAMIServ:: " + e.toString());
		  // e.printStackTrace();		 
	        return;
	    }

	    try {
	        login.getOptions(args);
	    } catch (AMI_UsageException e) {
	        System.out.println(e.toString());
	        login.usage();
	        return;
	    } catch (Exception e) {
	        System.out.println("AMI_LoginToAMIServ:: " + e.toString());
	 	// e.printStackTrace();		 
	        return;
	    }

	    switch (login.getFunctionality()) {

	    case LOGIN :
	        try {
		    login.registerKeysWithServer(login.getUserName(),
					       login.getPassword("amilogin"));
		} catch (Exception e) {
	            System.out.println("AMI_LoginToAMIServ:: " + e.toString());
		    // e.printStackTrace();		 
		}
		break;
	    case CHNG_PASSWORD:
	        try {
		    login.changeKeyStorePassword(login.getUserName(), 
			       login.getPassword("amilogin::OLD_PASSWORD:"),
			       login.getPassword("amilogin::NEW_PASSWORD:"));
		} catch (Exception e) {
	            System.out.println("AMI_LoginToAMIServ:: " + e.toString());
		   // e.printStackTrace();		 
		}
		break;

	    }

    }

    private Object[] retrieveKeyStore(String userName, String password) 
                       throws AMI_NoKeyStoreFoundException, Exception
    {
          Object[] ksArray = new Object[1];
          KeyStore ks = null;	  

	  // First, check if keystore specified at command line
	  if (_keystoreLocation.equals("FILE")) {
	    ksArray = new Object[_keystoreFileNames.size()];
	    for (int ii = 0; ii < _keystoreFileNames.size(); ii++) {
	    	ks = readKeyStoreFromFile(password, 
		  (String)_keystoreFileNames.elementAt(ii));
	    	ksArray[ii] = ks;
 	    }
	  }
	  // Second, check if smart card exists, if it does, read keystore
	  // from there. This is not supported in 1.0
	  else
	  if (smartCardExists()) {
	    ks = readKeyStoreFromSmartCard();
	    ksArray[0] = ks;
	  }	 
	  else {
	       // Otherwise, check if keystore present in directory service,
	       // or any other backend like file
	      
	    ks =  AMI_KeyMgntClient.getKeyStore(userName, password, _type);

	    if (ks == null) {
		_msgFormatter.applyPattern(_messages.
		getString("AMI_Cmd.login.noKeyStore"));
	        Object[] args = { new String(userName) };
	        throw new AMI_NoKeyStoreFoundException(
		_msgFormatter.format(args));
	    }

	    _rsaSignAlias = AMI_KeyMgntClient.
	    getRSASignAlias(userName, _type);
	    _rsaEncryptAlias = AMI_KeyMgntClient.
		getRSAEncryptAlias(userName, _type);
	    _dsaAlias = AMI_KeyMgntClient.getDSAAlias(userName, _type);
   	}
	    
	   ksArray[0] = ks;

	  // Return array containing keystores.
	  return ksArray;	 
    }


    public void changeKeyStorePassword(String userName,
					String oldPassword, String newPassword)
                                        throws Exception
    {
          // Change keystore password for file specified on command line
          String filename = null;
          AMI_Debug.debugln(1, "Changing password for KeyStore");
	 
	  try {
	    if (_keystoreLocation.equals("FILE"))
	    {
	       	for (int ii = 0; ii < _keystoreFileNames.size(); ii++) {
		   filename = (String)_keystoreFileNames.elementAt(ii);
	      	    changePasswordForFile((String)_keystoreFileNames.
			elementAt(ii), oldPassword, newPassword);
	       	}
	    }
	  } catch (Exception e) {
	        Object[] args = { new String(filename) };
	        _msgFormatter.applyPattern(_messages.
		getString("AMI_Cmd.login.cpFile")); 
	        throw new Exception(
		_msgFormatter.format(args) + e.toString());	      
	  }

	  // Change keystore password for all keystore in the configured backend
	  AMI_Debug.debugln(2,
	"Changing password for keystore in the configured backend");
	  
	  try {
	    changePasswordForDirService(userName, oldPassword, newPassword);
	  } catch (Exception e) {
	    throw new Exception(_messages.
	    getString("AMI_Cmd.login.cpNamingService") + 
				    e.toString());	      
	  }
	  
          // Register new keystores with server (AMI or SKI)
	  registerKeysWithServer(userName, newPassword);
          
    }

    public void registerKeysWithServer(String userName, String password) 
                                        throws Exception
    {
	  Object[] keyStores = null;

	  // Instansiate a client object, with the appropriate protocol
	  AMI_Debug.debugln(1,
	"AMI_loginToAMIServ::Getting a client object inst ");

	  AMI_KeyServ client = AMI_KeyServClient.
	getInstance(AMI_KeyMgntClient.
	getProperty(AMI_Constants.PROTOCOL_PROPERTY));
	  
	  // Retrieve Keystores ..

	  AMI_Debug.debugln(1, "Retrieving keystore for = " + userName);

	   keyStores = retrieveKeyStore(userName, password);

	  AMI_Debug.debugln(3, "Got keystore");

	  // FOR PAM ROOT LOGIN :: it is equivalent to amilogin -h 
	  if (userName.equals("root")) {
	    _hostLogin = true;
	    userName = InetAddress.getLocalHost().getHostName();
	    _hostIP = InetAddress.getLocalHost().getHostAddress();
	  }

	  AMI_Debug.debugln(3, "Creating entries vector");

	  // Create entries vector from the uids passed
	  if (_hostLogin) {
	    if (_uids.size() != 0) {
		    for (int ii = 0; ii < _uids.size(); ii++)
		        _entries.addElement(new AMI_EntryInfo(userName,
			_hostIP, Long.parseLong((String)_uids.elementAt(ii)),
			null));

	    }
	    else 
		    _entries.addElement(new AMI_EntryInfo(userName, _hostIP, 
							  0, "root"));
	  } else {
	    _entries.addElement(new AMI_EntryInfo(
			InetAddress.getLocalHost().getHostName(), 
			InetAddress.getLocalHost().getHostAddress(), 
		        getUserId(), userName));
	  }

	  // Register the keystores with the server
	  try {
	    AMI_Debug.debugln(3,
		"Removing previously registered keystore (if any)");

	       // remove previously registered keystores for this
	    client.setKeyStore(null, null, _entries, _permanent, null,
			  null, null);

	    AMI_Debug.debugln(3, "Registring keystores now ");

	    for (int ii = 0; ii < keyStores.length; ii++) {

	    	ByteArrayOutputStream  baos = new ByteArrayOutputStream();

		   ((KeyStore)keyStores[ii]).store(baos,
		   password.toCharArray());
		   client.setKeyStore(baos.toByteArray(), password, _entries, 
			    _permanent, _rsaSignAlias, _rsaEncryptAlias,
			    _dsaAlias);

		   System.out.println(_messages.getString(
		   "AMI_Cmd.login.success"));
	    }
	  } catch (Exception e) {
	    throw new Exception(_messages.
	    getString("AMI_Cmd.login.register") + e.toString());
	  }
	  
    }

    public String GUIregisterKeysWithServer(String userName, String password) 
                                        throws Exception  {
	  Object[] keyStores = null;

	  // Instansiate a client object, with the appropriate protocol

	  AMI_KeyServ client = AMI_KeyServClient.getInstance(
         	 AMI_KeyMgntClient.
		getProperty(AMI_Constants.PROTOCOL_PROPERTY));
	  
	  // Retrieve Keystores ..

	   keyStores = retrieveKeyStore(userName, password);


	  // FOR PAM ROOT LOGIN :: it is equivalent to amilogin -h 
	if (userName.equals("root")) {
	    _hostLogin = true;
	    userName = InetAddress.getLocalHost().getHostName();
	    _hostIP = InetAddress.getLocalHost().getHostAddress();
	}

	// Create entries vector from the uids passed
	if (_hostLogin) {
	    if (_uids.size() != 0) {
		    for (int ii = 0; ii < _uids.size(); ii++)
		        _entries.addElement(new AMI_EntryInfo(userName,
			_hostIP, Long.parseLong((String)_uids.
			elementAt(ii)), null));

            } else 
		    _entries.addElement(new AMI_EntryInfo(userName, _hostIP, 
							  0, "root"));
	} else {
	    _entries.addElement(new AMI_EntryInfo(
		InetAddress.getLocalHost().getHostName(), 
		InetAddress.getLocalHost().getHostAddress(), 
	        getUserId(), userName));
	}

	// Register the keystores with the server
	try {

	       // remove previously registered keystores for this
		client.setKeyStore(null, null, _entries, _permanent, null,
				  null, null);

	       	for (int ii = 0; ii < keyStores.length; ii++) {

		    ByteArrayOutputStream  baos = new ByteArrayOutputStream();

		    ((KeyStore)keyStores[ii]).
			store(baos, password.toCharArray());
		    client.setKeyStore(baos.toByteArray(), password, _entries, 
				    _permanent, _rsaSignAlias, _rsaEncryptAlias,
				    _dsaAlias);
		
		    return (new String("SUCCESS"));
	       	}
	} catch (Exception e) {
		   return (new String("FAIL "+e.toString()));
  	}
	return ("FAIL Fell through");
	  
    }

    private String _rsaSignAlias = null;
    private String _rsaEncryptAlias = null;
    private String _dsaAlias = null;

    private Vector _entries;

}
