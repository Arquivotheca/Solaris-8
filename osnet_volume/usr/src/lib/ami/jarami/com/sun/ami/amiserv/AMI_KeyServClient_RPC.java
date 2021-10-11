/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_KeyServClient_RPC.java	1.2 99/07/18 SMI"
 *
 */

package com.sun.ami.amiserv;

import java.util.Vector;
import java.io.IOException;

import com.sun.ami.AMI_Exception;
import com.sun.ami.common.*;
import com.sun.ami.utils.AMI_C_Certs;

public class AMI_KeyServClient_RPC extends AMI_Common implements AMI_KeyServ {

        private native int initRPC();

        private native byte[] signDataRPC(String hostIP, String hostName, 
		long uid, String userName, byte[] input, 
		String algorithm, String keyAlias) throws AMI_Exception;
  
        private native byte[] unwrapDataRPC(String hostIP, String hostName, 
		long uid, String userName, byte[] input, 
		String algorithm, String keyAlias) throws AMI_Exception;

        private native byte[] getKeyStoreRPC(String hostIP, String hostName, 
			 long uid, String userName) throws AMI_Exception;

        private native void setKeyStoreRPC(byte[]keystore, String password, 
		Object[] uids, int permanent, String rsaSignAlias, 
		String rsaEncryptAlias, String dsaAlias)
		throws AMI_Exception;
  
	public AMI_KeyServClient_RPC() throws AMI_Exception {
		try {
			AMI_Debug.debugln(3, "Instantiating RPC Client");
		} catch (IOException e) {
			throw new AMI_Exception(e.getMessage());
		}	     

		if (initRPC() != 0) {
			try {
				AMI_Debug.debugln(3,
				    "initRPC JNI returned error");
			} catch (IOException e) {
			}	     
			throw new AMI_Exception(
			    "Unable to initialize RPC Client");
		}
	}

        /*
	* Register a key store with the AMI Server
	* @param keystore The Key store object to be registered
	* @param keyStoreType The kind of keys present in this keystore(RSA/DSA)
	* @param password The password used to encrypt this keystore
	* @param entries All the user/hosts for which this keystore should 
	*        be registered
	* @param permanent Whether this key should be written to
	* permanent storage
	* @param rsaSignAlias Default signature alias for RSA
	* @param rsaEncryptAlias Default encryption alias for RSA
	* @param dsaAlias Default alias for DSA 
	* 
	* @throws RemoteException
	*/
	
        public void setKeyStore(byte[] keystore, 
	    String password, Vector entries, int permanent,
	    String rsaSignAlias, String rsaEncryptAlias, String dsaAlias) 
	    throws AMI_Exception {
		AMI_EntryInfo[] userinfo = new AMI_EntryInfo[entries.size()];

		entries.copyInto(userinfo);

		try {
			AMI_Debug.debugln(3, "Invoking setkeystore JNI call");
		} catch (IOException e) {
			throw new AMI_Exception(e.getMessage());
		}	     

		// System.out.println("IN Java rsaSignAlias = " +
		// rsaSignAlias);
		setKeyStoreRPC(keystore, password, userinfo,
		    permanent, rsaSignAlias,
		    rsaEncryptAlias, dsaAlias);
	}
	

        public byte[] signData(AMI_CryptoInfo info) 
	    throws AMI_Exception {

		byte[] signedData = null;

		AMI_EntryInfo entry = info.getEntry();
		signedData = signDataRPC(entry.getHostIP(),
		    entry.getHostName(), 
		    entry.getUserId(), entry.getUserName(), 
		    info.getInput(), info.getAlgorithm(), 
		    info.getKeyAlias());

		return signedData;	    
	}

	public byte[] unwrapData(AMI_CryptoInfo info) throws AMI_Exception {
		byte[] unwrapped = null;
		AMI_EntryInfo entry = info.getEntry();
		unwrapped = unwrapDataRPC(entry.getHostIP(),
		    entry.getHostName(),
		    entry.getUserId(), entry.getUserName(), 
		    info.getInput(), info.getAlgorithm(), 
		    info.getKeyAlias());

		return unwrapped;	    
	}

	public byte[] getKeyStore(AMI_EntryInfo info) throws AMI_Exception {
		byte[] keystore = null;

		keystore = getKeyStoreRPC(info.getHostIP(),
		    info.getHostName(), info.getUserId(), info.getUserName());
		return keystore;	    

	}

	public String getKeyPkg(AMI_EntryInfo info) throws AMI_Exception {
		return (null);
	}

	/* Function to get the trusted certificates */
	public AMI_C_Certs[] getTrustedCertificates(AMI_EntryInfo info)
	    throws AMI_Exception {
		return (null);
	}

	/* Function to change the password of KeyStore */
	public byte[] changeKeyStorePassword(String oldPasswd,
	    String newPasswd, byte[] keystore) throws AMI_Exception {
		return (null);
	}
}
