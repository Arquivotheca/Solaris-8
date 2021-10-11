/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_KeyServ.java	1.2 99/07/18 SMI"
 *
 */

package com.sun.ami.amiserv;

import java.util.Vector;

import com.sun.ami.AMI_Exception;
import com.sun.ami.utils.AMI_C_Certs;

/** 
 * This is the interface file for the AMI private key operations 
 * remote object.
 * 
 *
 * @author     Sangeeta Varma
 */

public interface AMI_KeyServ {

	/*
	 * Register a key store with the AMI Server
	 * @param keystore The Key store object to be registered
	 * @param keyStoreType The kind of keys present in this
	 * keystore (RSA/DSA)
	 * @param password The password used to encrypt this keystore
	 * @param entries All the user/hosts for which this keystore should 
	 *        be registered
	 * @param permanent Whether this key should be written
	 * to permanent storage
	 * @param rsaSignAlias Default signature alias for RSA
	 * @param rsaEncryptAlias Default encryption alias for RSA
	 * @param dsaAlias Default alias for DSA 
	 * 
	 * @throws AMI_Exception
	 */

        void setKeyStore(byte[] keystore, 
	    String password, Vector entries, int permanent,
	    String rsaSignAlias, String rsaEncryptAlias,
	    String dsaAlias) throws AMI_Exception;

	/*
	 * Retrieve a particular keystore, previously registered with the
	 * AMI Server.
	 * @param info The user/host information for the keystore which
	 *        you are trying to retrieve
	 * @returns KeyStore The Keystore object
	 * @throws AMI_Exception
	 */
	
	byte[] getKeyStore(AMI_EntryInfo info) throws AMI_Exception;

	/* Function to get the algorithm of the key alias */
	String getKeyPkg(AMI_EntryInfo info) throws AMI_Exception;

	/* Function to get the trusted certificates */
	AMI_C_Certs[] getTrustedCertificates(AMI_EntryInfo info)
	    throws AMI_Exception;

	/*
	 * Sign the data using the private key from the AMI Server.
	 * @param info All the information required to sign the data 
	 * (see AMI_CryptoInfo object)
	 * @returns byte[] Containing the signed data.
	 *
	 * @throws AMI_Exception
	 */

	byte[] signData(AMI_CryptoInfo info) throws AMI_Exception;

	/*
	 * Unwrap the symmetric key using the private key from the AMI Server.
	 * @param info All the information required to unwrap the data 
	 * (see AMI_CryptoInfo object)
	 * @returns byte[] Containing the unwrapped key.
	 *
	 * @throws AMI_Exception
	 */
	byte[] unwrapData(AMI_CryptoInfo info) throws AMI_Exception;

	/* Function to change the password of KeyStore */
	byte[] changeKeyStorePassword(String oldPasswd, String newPasswd,
	    byte[] keystore) throws AMI_Exception;
}
