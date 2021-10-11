/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * #pragma ident "@(#)AMI_Login.java	1.1 99/07/11 SMI"
 *
 */


package com.sun.ami.cmd;

import java.lang.*;
import java.util.*;
import java.io.*;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.security.KeyStore;
import java.security.PrivateKey;
import java.text.MessageFormat;
import sun.misc.BASE64Decoder;

import com.sun.ami.AMI_Exception;
import com.sun.ami.common.*;
import com.sun.ami.utils.AMI_C_Utils;
import com.sun.ami.utils.AMI_GetOpt;
import com.sun.ami.keymgnt.AMI_KeyMgntService;
import com.sun.ami.keymgnt.AMI_KeyMgntClient;
import com.sun.ami.keymgnt.AMI_KeyMgntException;

/**
 * This class provides a utility to login with the AMI/SKI Server.
 * The implementation class for this , should implement the
 * registerKeysWithServer and changeKeyStorePassword methods.
 *
 * @author Sangeeta Varma
 *
 * @version 1.0
 *
 */

public abstract class AMI_Login extends AMI_Common
    implements AMI_KeyMgntService {

    private static final String BEGIN_KEYSTORE = "-----BEGIN KEYSTORE-----";
    private static final String END_KEYSTORE = "-----END KEYSTORE-----";

    protected static final int LOGIN = 0;
    protected static final int CHNG_PASSWORD = 1;

    protected String cmdPassword = null;

    public AMI_Login() throws Exception {

	_props = System.getProperties();
	_uids = new Vector();
        // Initialise the Locale
        Locale currentLocale = AMI_Constants.initLocale();
        _msgFormatter = new MessageFormat("");
        _msgFormatter.setLocale(currentLocale);
        _messages = AMI_Constants.getMessageBundle(currentLocale);
    }

    private void initUserName() {
          _name = _props.getProperty("user.name");
    }

    /*
     * Determines if input is binary or Base64 encoded.
     */
    private boolean isBase64(InputStream is) throws IOException {
        if (is.available() >= 10) {
            is.mark(10);
            int c1 = is.read();
            int c2 = is.read();
            int c3 = is.read();
            int c4 = is.read();
            int c5 = is.read();
            int c6 = is.read();
            int c7 = is.read();
            int c8 = is.read();
            int c9 = is.read();
            int c10 = is.read();
            is.reset();
            if (c1 == '-' && c2 == '-' && c3 == '-' && c4 == '-'
                && c5 == '-' && c6 == 'B' && c7 == 'E' && c8 == 'G'
                && c9 == 'I' && c10 == 'N') {
                return true;
            } else {
                return false;
            }
        } else {
            throw new IOException("Cannot determine encoding format");
        }
    }

    /*
     * Reads the entire input stream into a byte array.
     */
    private byte[] getTotalBytes(InputStream is) throws IOException {
        byte[] buffer = new byte[8192];
        ByteArrayOutputStream baos = new ByteArrayOutputStream(2048);
        int n;
        baos.reset();
        while ((n = is.read(buffer, 0, buffer.length)) != -1) {
            baos.write(buffer, 0, n);
        }
        return baos.toByteArray();
    }

    private byte[] base64_to_binary(InputStream is) throws IOException {
       
        byte            keystore[];
        StringBuffer    line = new StringBuffer();
        String          Sline;
        int             rc;

        BufferedReader myBufferReader = new BufferedReader
                        (new InputStreamReader(is));
        Sline = myBufferReader.readLine();

        rc = Sline.compareTo(BEGIN_KEYSTORE);
        if (rc == 0)
        {
              while ((Sline = myBufferReader.readLine()).
                     compareTo(END_KEYSTORE) != 0)
             {
                    line.append(Sline);

              }
        } else {
              do {
                     line.append(Sline);
                     Sline = myBufferReader.readLine();
                     if (Sline != null)
                     {
                           rc = Sline.compareTo(END_KEYSTORE);

                      } else rc = 0;
                 } while (rc != 0);
        }

         BASE64Decoder base64 = new BASE64Decoder();
         keystore = base64.decodeBuffer(line.toString());

         myBufferReader.close();
         return keystore;
    }

    protected void writeKeyStoreToFile(KeyStore ks, String password,
	String filename) throws Exception 
    {
          OutputStream os = new FileOutputStream(filename);
	  ks.store(os, password.toCharArray());
    }

    protected KeyStore readKeyStoreFromFile(String password, String filename)
                                           throws Exception 
    {
	  InputStream is = new FileInputStream(filename);
	  KeyStore ks = KeyStore.getInstance("jks");

	  if (is.markSupported() == false) {
		 // consume the entire input stream
		 byte[] totalBytes;
		 totalBytes = getTotalBytes(new BufferedInputStream(is));
		 is = new ByteArrayInputStream(totalBytes);
	  };

	  if (isBase64(is)) {
		// Base64
		 byte[] data = base64_to_binary(is);
		 ks.load(new ByteArrayInputStream(data),
		password.toCharArray());
	  } else {
	        // binary
		ks.load(is, password.toCharArray());
	  }

	  return ks;
    }

    // Dummy function , for future smart card support
    protected boolean smartCardExists() {
          return false;
    }

    // Dummy function , for future smart card support
    protected KeyStore readKeyStoreFromSmartCard() {
          return null;
    }

    protected String getUserName() {
	 // Get current users name 
	 return _name;
    }

    public String getNameLogin() {
	 // Get current users name 
	 return _name;
    }

    public String getNameDN() {
        // Get current users dn name 
        try {
            return AMI_KeyMgntClient.getDNNameFromLoginName(_name, _type);
        } catch (Exception e) {
	    return null;
        }
    }

    public String getNameDNS()   {
       // Get current users dns name 
        try {
           return AMI_KeyMgntClient.getDNSNameFromLoginName(_name, _type);
        } catch (Exception e) {
	  return null;
        }
    }

    protected int getFunctionality()  
    {
           return _func;
    }

    protected long getUserId() throws AMI_Exception 
    {
	   // Get current users id 
	   return AMI_C_Utils.ami_get_user_id();
    }

    protected String getPassword(String prompt)
	throws AMI_Exception, IOException {

           String keypasswd = null;

	   if (cmdPassword != null)
		return (cmdPassword);

	   BufferedReader reader = new BufferedReader(
		new InputStreamReader(System.in));

	   if (_hostLogin) {
		keypasswd = AMI_C_Utils.ami_get_password(prompt, 
	   					 AMI_Constants.AMI_HOST_LOGIN);
	   } else {
	       	keypasswd = AMI_C_Utils.ami_get_password(prompt,
	   				 AMI_Constants.AMI_USER_LOGIN);
	   }
	   AMI_Debug.debugln(3, "Read password using c rotuine");
	   return keypasswd;
    }


    protected void getOptions(String[] argv) throws AMI_UsageException,
	UnknownHostException, IOException, AMI_Exception
    {
	AMI_GetOpt go = new AMI_GetOpt(argv, "hpL:z:y:c");
	    int ch = -1;
	    boolean isVirtualHost = false;
	    initUserName();

	    while ((ch = go.getopt()) != go.optEOF) {

                /*
                 * options: -z keystorefilename,
		 *          -h < uids >
		 *          -L ipaddress or hostnames
		 *          -p permanently register users keys
                 */

		if ((char)ch == 'z') {
		     _keystoreLocation = "FILE";
		     _keystoreFileNames.addElement(go.optArgGet());
	 	} else if ((char)ch == 'h') { 
		     if (!getUserName().equals("root")) {
		        throw new 
			  AMI_UsageException(_messages.getString(
				"AMI_Cmd.login.usage1"));
		     }
		     _hostLogin = true;
		     _type = AMI_Constants.AMI_HOST_OBJECT;
		     _permanent  |= AMI_Constants.AMI_PERMANENT;
		     if (!isVirtualHost) {
		          _name = InetAddress.getLocalHost().getHostName();
			  _hostIP = InetAddress.getLocalHost().getHostAddress();
		     }

		} else if ((char) ch == 'y') { 
		    // Password in the command line
		    cmdPassword = go.optArgGet();
	 	} else if ((char)ch == 'L') { 
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
		} else 
		    if ((char)ch == 'p') {
		    _permanent  |= AMI_Constants.AMI_PERMANENT;
		} else
		    if ((char)ch == 'c') {
		    _func = CHNG_PASSWORD;
		}
		else
		    throw new AMI_UsageException("");
	    }

	    if (go.optIndexGet() < argv.length) {
	        if (_hostLogin)
		    for (int k = go.optIndexGet(); k < argv.length; k++) 
		        // This is valid only if host (-h) 
			// option has been specified
		        _uids.addElement(argv[k]);		     
		 else 
		   throw new AMI_UsageException("");
	    }

	    if (getUserName().equals("root") && (!_hostLogin))
	        throw new AMI_UsageException(_messages.getString(
		"AMI_Cmd.login.usage2"));    
	    if ((_hostIP != null) && (_permanent == 0))
	        throw new AMI_UsageException(_messages.
		getString("AMI_Cmd.login.usage3"));
    }
     
    protected void usage() {
           System.err.println(_messages.getString("AMI_Cmd.login.usage"));
    }

    private KeyStore changePassword(KeyStore keystore, String oldPassword,
				 String newPassword) throws Exception 
    {
           String alias;
           KeyStore answer = KeyStore.getInstance("jks");
           answer.load(null, null);
           Enumeration enum = keystore.aliases();
                
	   while (enum.hasMoreElements()) {
                     
		alias = (String) enum.nextElement();
                if (keystore.isKeyEntry(alias)) {
                    answer.setKeyEntry(alias,
                    (PrivateKey)(keystore.getKey(alias,
		    oldPassword.toCharArray())),
	 	   newPassword.toCharArray(), 
					  keystore.getCertificateChain(alias));
                } else {
                   answer.setCertificateEntry(alias,
                                    keystore.getCertificate(alias));
                }
	    }
            return (answer);
    }

    protected void changePasswordForDirService(String userName, 
      String oldPassword, String newPassword) throws Exception
    {
           // Change the password for the keystores
           KeyStore keystore = AMI_KeyMgntClient.getKeyStore(userName,
						oldPassword, _type);
           if (keystore != null) { 
                   keystore = changePassword(keystore, oldPassword,
		   newPassword);
                   if (keystore != null) 
                        AMI_KeyMgntClient.setKeyStore(this, userName,
			"keystoreRSA", keystore, newPassword, _type); 
           }
    }

    protected void changePasswordForFile(String fileName, 
		    String oldPassword, String newPassword)
                    throws Exception
    {
          KeyStore ks = readKeyStoreFromFile(oldPassword, fileName);

  	  ks = changePassword(ks, oldPassword, newPassword);
	  writeKeyStoreToFile(ks, newPassword, fileName);

	  return;
    }

    protected abstract void changeKeyStorePassword(String userName, 
						    String old_password,
						    String new_password) 
                                            throws Exception;

    protected abstract void registerKeysWithServer(String userName, String
					    password) 
                                            throws Exception;

    private String _name = null;  // User or Host Name

    protected String  _keystoreLocation = "NAMING_SERVICE";
    protected Vector  _keystoreFileNames = new Vector();
    protected boolean _hostLogin = false;
    protected String  _hostIP = null;
    protected int     _permanent = 0;
    protected String  _type = AMI_Constants.AMI_USER_OBJECT;
    protected int     _func = LOGIN;
    protected Vector  _uids = null;

    protected Properties     _props;    
    protected ResourceBundle _messages;
    protected MessageFormat  _msgFormatter;
}
