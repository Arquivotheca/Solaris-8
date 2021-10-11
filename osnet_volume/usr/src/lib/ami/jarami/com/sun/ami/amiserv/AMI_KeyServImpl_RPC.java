/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_KeyServImpl_RPC.java	1.2 99/07/18 SMI"
 *
 */

package com.sun.ami.amiserv;

import java.util.Vector;
import com.sun.ami.AMI_Exception;
import com.sun.ami.common.AMI_Debug;
import com.sun.ami.utils.AMI_C_Certs;

public class AMI_KeyServImpl_RPC
{

	protected AMI_KeyServ keyserv;

        public AMI_KeyServImpl_RPC() {
		try {
			keyserv = new AMI_KeyServImpl();
		} catch (AMI_Exception e) {
			debugMessage(e.getMessage());
		}
	}

	public int setKeyStore(byte[] keyStoreArray, String password,
	    AMI_EntryInfo[] entry, int permanent, String rsaSignAlias,
	    String rsaEncryptAlias, String dsaAlias) {
		int ret = 1;
		Vector entries = new Vector();
		for (int i = 0; i < entry.length; i++)
			entries.add(entry[i]);
		try {
			keyserv.setKeyStore(keyStoreArray, password,
			    entries, permanent, rsaSignAlias, rsaEncryptAlias,
			    dsaAlias);
		} catch (AMI_Exception e) {
			ret = 0;
			debugMessage(e.getMessage());
		}
		return (ret);
	}

	public void listKeyStores() {
		// %%% NOT implemented
		// try {
		//	keyserv.listKeyStores();
		// } catch (AMI_Exception e) {
		//	throw new RemoteException(e.getMessage());
		// }
	}

	public byte[] getKeyStore(AMI_EntryInfo info) {
		try {
			return (keyserv.getKeyStore(info));
		} catch (AMI_Exception e) {
			debugMessage(e.getMessage());
		}
		return (null);
	}

	public String getKeyPkg(AMI_EntryInfo info) {
		try {
			return (keyserv.getKeyPkg(info));
		} catch (AMI_Exception e) {
			debugMessage(e.getMessage());
		}
		return (null);
	}

	public byte[] signData(AMI_CryptoInfo info) {
		try {
			return (keyserv.signData(info));
		} catch (AMI_Exception e) {
			debugMessage(e.getMessage());
		}
		return (null);
	}

	public byte[] unwrapData(AMI_CryptoInfo info) {
		try {
			return (keyserv.unwrapData(info));
		} catch (AMI_Exception e) {
			debugMessage(e.getMessage());
		}
		return (null);
	}

	/* Function to get the trusted certificates */
	public AMI_C_Certs[] getTrustedCertificates(AMI_EntryInfo info)
	    throws AMI_Exception {
		return (keyserv.getTrustedCertificates(info));
	}

	/* Function to change the password of KeyStore */
	public byte[] changeKeyStorePassword(String oldPasswd,
	    String newPasswd, byte[] keystore) throws AMI_Exception {
		return (keyserv.changeKeyStorePassword(oldPasswd,
		    newPasswd, keystore));
	}

	protected void debugMessage(String message) {
		try {
			AMI_Debug.debugln(1, message);
		} catch (Exception e) {
			// e.printStackTrace();
		}
	}
}
