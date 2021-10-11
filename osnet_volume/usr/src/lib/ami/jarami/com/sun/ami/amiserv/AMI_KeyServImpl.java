/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_KeyServImpl.java	1.4 99/07/18 SMI"
 *
 */

package com.sun.ami.amiserv;

import java.util.*;
import java.io.*;
import java.text.MessageFormat;
import java.security.*;
import java.security.cert.Certificate;
import java.security.cert.X509Certificate;
import sun.security.util.DerValue;
import sun.security.util.DerOutputStream;
import sun.security.util.DerInputStream;
import sun.security.util.BigInt;
import sun.misc.BASE64Encoder;
import sun.misc.BASE64Decoder;

import com.sun.ami.AMI_Exception;
import com.sun.ami.common.*;
import com.sun.ami.keygen.AMI_PrivateKey;
import com.sun.ami.keymgnt.AMI_KeyStore;
import com.sun.ami.cmd.AMI_NoKeyStoreFoundException;
import com.sun.ami.crypto.AMI_Crypto;
import com.sun.ami.utils.AMI_C_Certs;

/** 
 * This class implements all the private key operations which are
 * done by the AMI Server. This object is created and registered as 
 * a remote object. All the methods are invoked by the clients as RMI
 * calls.
 * 
 * @author     Sangeeta Varma
 * @version 1.0
 *
 */

public class AMI_KeyServImpl implements AMI_KeyServ {

        protected static final String SEPERATOR = "#";

        public AMI_KeyServImpl() throws AMI_Exception {
		super();
		if (!_initialised) {
			try {
				init();
			} catch (Exception e) {
				throw new AMI_Exception(e.getMessage());
			}
		}
	}

	protected void init() throws Exception {
		// Mutex operation
		synchronized (this) {
			if (_initialised)
				return;
			_initialised = true;
		}

		// Set the providers, if not already installed.
	       	if (Security.getProvider("SunAMI") == null) {
			Security.insertProviderAt(new SunAMI(), 1);
		}		

		// Set required variables
		_keyStores = new Vector();
		Locale currentLocale = AMI_Constants.initLocale();
		_msgFormatter = new MessageFormat("");
		_msgFormatter.setLocale(currentLocale);
		_messages = AMI_Constants.getMessageBundle(
		    currentLocale);

		// Initialise Permanent keys
		File file2 = new File(AMI_Constants.AMI_PERM_KEYS_FILENAME);
		if (file2.exists()) {
			// Read all entires from the file into a list.
			BufferedReader reader = new BufferedReader(
			    new InputStreamReader(new FileInputStream(file2)));
			_keyStores = readKeyStore(reader);
			if (_keyStores.size() == 0) {
				AMI_Debug.debugln(3,
				    _messages.getString(
					"AMI_Cmd.server.nokeys"));
			} else {
				_msgFormatter.applyPattern(
				    _messages.getString(
				    "AMI_Cmd.server.permkeys"));
				Object[] args =
				    { new Integer(_keyStores.size()) };
				AMI_Debug.debugln(3,
				    _msgFormatter.format(args));
			}
		} else {
			AMI_Debug.debugln(3,
			    _messages.getString("AMI_Cmd.server.nokeys"));
		}
	}

        public void setKeyStore(byte[] keyStoreArray, String password, 
	    Vector entries, int permanent, String rsaSignAlias,
	    String rsaEncryptAlias, String dsaAlias) throws AMI_Exception {
		String alias;
		KeyStore keyStore = null;
		KeyStore keyStoreRSA = null;
		boolean rsaKeysPresent = false;
		KeyStore keyStoreDSA = null;
		boolean dsaKeysPresent = false;
		Enumeration aliases = null;

		try {
			keyStore = KeyStore.getInstance("jks");
			if (keyStoreArray != null) {
				keyStore.load(new ByteArrayInputStream(
				    keyStoreArray), password.toCharArray());
			} else {
				keyStore.load(null, null);
			}

			if (keyStore.size() == 0) {
				AMI_Debug.debugln(3,
				    "AMI_KeyServImpl::setKeyStore: " +
				    "null keystore");
				setTypedKeyStore(null, "RSA", password,
				    entries, permanent, null, null, null);
				setTypedKeyStore(null, "DSA", password,
				    entries, permanent, null, null, null);
				return;
			}

			keyStoreRSA = KeyStore.getInstance("jks");
			keyStoreDSA = KeyStore.getInstance("jks");
			keyStoreRSA.load(null, null);
			keyStoreDSA.load(null, null);

			// Set the Key Entry into the appropriate keystore,
			// based on key type.
			aliases = keyStore.aliases();
			while (aliases.hasMoreElements()) {
				alias = (String) aliases.nextElement();
				if (keyStore.isKeyEntry(alias)) {
					Key key = keyStore.getKey(
					    alias, password.toCharArray());
					Certificate[] certs =
					    keyStore.getCertificateChain(alias);

					if (key.getAlgorithm().equals("RSA")) {
						keyStoreRSA.setKeyEntry(alias,
						    key,
						    password.toCharArray(),
						    certs);
						rsaKeysPresent = true;
					} else if (key.getAlgorithm().equals(
					    "DSA")) {
						keyStoreDSA.setKeyEntry(alias,
						    key,
						    password.toCharArray(),
						    certs);
						dsaKeysPresent = true;
					}
				} else {
					Certificate cert =
					    keyStore.getCertificate(alias);
					keyStoreRSA.setCertificateEntry(alias,
					    cert);
					keyStoreDSA.setCertificateEntry(alias,
					    cert);
				}
			}

			// Set the RSA keystore
			if (rsaKeysPresent)
				setTypedKeyStore(keyStoreRSA, "RSA", password,
				    entries, permanent, rsaSignAlias,
				    rsaEncryptAlias, dsaAlias);

			// Set the DSA keystore
			if (dsaKeysPresent)
				setTypedKeyStore(keyStoreDSA, "DSA", password,
				    entries, permanent, rsaSignAlias,
				    rsaEncryptAlias, dsaAlias);
		} catch (Exception e) {
			throw new AMI_Exception(e.toString());
		}
	}

	protected void setTypedKeyStore(KeyStore keyStore, String keyStoreType,
	    String password, Vector entries, int permanent, String rsaSignAlias,
	    String rsaEncryptAlias, String dsaAlias) throws Exception {

		AMI_Store store = null;

		if ((entries == null) || (entries.size() == 0))
			throw new AMI_Exception(_messages.getString(
			    "AMI_Cmd.server.noentries"));

		for (int ii = 0; ii < entries.size(); ii++) {
			// Check if a keystore already exists for this
			// combination of ip/id and keytype. If present delete
			try {
			        store = findKeyStore(keyStoreType,
				    (AMI_EntryInfo) entries.elementAt(ii));

                                if (store != null) {
                                        AMI_Debug.debugln(3,
					"setKeyStore::Previous " +
					"key store found");
                                        deleteKeyStore(store);
                                }
			} catch (AMI_NoKeyStoreFoundException e) {
			}

			if (keyStore != null)
				addKeyStore(keyStore, keyStoreType, password,
			    		(AMI_EntryInfo) entries.elementAt(ii),
					permanent, rsaSignAlias,
					rsaEncryptAlias, dsaAlias);
		}
	}

        public byte[] getKeyStore(AMI_EntryInfo info) throws AMI_Exception {
	    try {
		ByteArrayOutputStream baos = new ByteArrayOutputStream();
		KeyStore retKeyStore = getInternalKeyStore(info);
		retKeyStore.store(baos, "".toCharArray());
		return baos.toByteArray();
	    } catch (Exception e) {
		// e.printStackTrace();
	    }
	    return (null);
	}

	protected KeyStore getInternalKeyStore(AMI_EntryInfo info)
	    throws AMI_Exception {
		AMI_Store rsaStore = null, dsaStore = null;
		KeyStore retKeyStore = null;
		try {
			AMI_KeyStore.setServer();
			retKeyStore = KeyStore.getInstance("amiks", "SunAMI");
			retKeyStore.load(null, null);
	   
			try {
				rsaStore = findKeyStore("RSA", info);
			} catch (AMI_NoKeyStoreFoundException e) {
				// Ignore this exception ...
			}
			if (rsaStore != null) {
				AMI_Debug.debugln(3,
				    "Populating with RSA keystore ..");
				populateKeyStore(retKeyStore,
				    rsaStore.getKeyStore(),
				    rsaStore.getPassword());
				AMI_Debug.debugln(3,
				    "getKeyStore::completed RSA");
			}

			try {
				dsaStore = findKeyStore("DSA", info);
			} catch (AMI_NoKeyStoreFoundException e) {
				// Ignore this exception..
			}	    
			if (dsaStore != null) {
				AMI_Debug.debugln(3,
				    "Populating with DSA keystore ..");
				populateKeyStore(retKeyStore,
				    dsaStore.getKeyStore(),
				    dsaStore.getPassword());
				AMI_Debug.debugln(3,
				    "getKeyStore:: completed DSA");
			}

			if (rsaStore == null && dsaStore == null) {
				AMI_Debug.debugln(3,
				    "getKeyStore:: No keystore found for : " +
				    info);
			} else {
				AMI_Debug.debugln(3,
				    "getKeyStore:: Returning " +
				    "keystore of size = " +
				    retKeyStore.size());
			}
			AMI_Debug.debugln(3, "getKeyStore:: store complete");
		} catch (Exception e) {
			throw new AMI_Exception(e.toString());
		}
		return (retKeyStore);
	}

        public String getKeyPkg(AMI_EntryInfo info) throws AMI_Exception {
		String keyAlias = info.getHostName();
		KeyStore ks;
		PrivateKey privateKey = null;
		AMI_Store store = null;

		// First try for RSA
		try {
			store = findKeyStore("RSA", info);
			ks = store.getKeyStore();

			privateKey = (PrivateKey) ks.getKey(
			    keyAlias, (store.getPassword()).toCharArray());
			if (privateKey != null)
				return (privateKey.getAlgorithm());
		} catch (Exception e) {
			// e.printStackTrace();
		}

		// Try DSA
		try {
			store = findKeyStore("DSA", info);
			ks = store.getKeyStore();

			privateKey = (PrivateKey) ks.getKey(
			    keyAlias, (store.getPassword()).toCharArray());
			if (privateKey != null)
				return (privateKey.getAlgorithm());
		} catch (Exception e) {
			// e.printStackTrace();
		}
		return (null);
	}

        public void listKeyStores() throws Exception {
		// for (int i = 0; i < _keyStores.size(); i++) {
			// System.out.println((AMI_Store)
			//    (_keyStores.elementAt(i)));
		// }
	}

        public byte[] signData(AMI_CryptoInfo info) throws AMI_Exception {
		byte[] signedData = null;
		PrivateKey privateKey = null;
		Signature  signObj = null;
		byte[] signature = null;

		try {
			AMI_Debug.debugln(3, "In signData server method");
		} catch (Exception exp) {}

		// Find the appropriate AMI_Store object for this signature.
		if (info.getAlgorithm().equals("MD5/RSA") ||
		    info.getAlgorithm().equals("MD2/RSA") ||
		    info.getAlgorithm().equals("SHA1/RSA")) {
			try {
				privateKey = findPrivateKey("RSA",
				    info.getKeyAlias(), info.getEntry(), 
				    AMI_Constants.SIGN);
			} catch (Exception e) {
				try {
					AMI_Debug.debugln(3,
					    "Unable to find private key");
				} catch (Exception exp) {}
				throw new AMI_Exception(e.toString());
			}
		} else  if (info.getAlgorithm().equals("SHA/DSA")) {
			try {
				privateKey = findPrivateKey("DSA",
				    info.getKeyAlias(),	info.getEntry(), 
				    AMI_Constants.SIGN);
 			} catch (Exception e) {
				// e.printStackTrace();
				throw new AMI_Exception(e.getMessage());
			}	 
		} else {
			_msgFormatter.applyPattern(_messages.getString(
			    "AMI_Cmd.server.unsupportedAlgo"));
			Object[] args = { new String(info.getAlgorithm()) };
			throw new AMI_Exception(_msgFormatter.format(args));
		}

		// Invoke the signature methods based on the algorithms passed
		try {
			signObj = Signature.getInstance(info.getAlgorithm());
			signObj.initSign(privateKey);
			signObj.update(info.getInput(), 0,
			    info.getInput().length);
			signedData = signObj.sign();
		} catch (NoSuchAlgorithmException e) {
			// e.printStackTrace();
			throw new AMI_Exception(e.getMessage());
		} catch (InvalidKeyException f) {
			// f.printStackTrace();
			throw new AMI_Exception(f.getMessage());
		} catch (SignatureException g) {
			// g.printStackTrace();
			throw new AMI_Exception(g.getMessage());
		}
		return signedData;
	}


	public byte[] unwrapData(AMI_CryptoInfo info) throws AMI_Exception {
		byte[] outputData = null;
		PrivateKey privateKey = null;
		
		try {
			AMI_Debug.debugln(3,
			    "In unwrapData server method : " + info);
		} catch (Exception exp) {}

		if (info.getAlgorithm().equals("RSA")) {
			try {
				privateKey = findPrivateKey("RSA",
				    info.getKeyAlias(), info.getEntry(),
				    AMI_Constants.ENCRYPT);
			} catch (Exception e) {
				// e.printStackTrace();
				throw new AMI_Exception(e.toString());
			}
		} else {
			_msgFormatter.applyPattern(_messages.getString(
			    "AMI_Cmd.server.unsupportedAlgo"));
			Object[] args = { new String(info.getAlgorithm()) };
			throw new AMI_Exception(_msgFormatter.format(args));
		}

		try {
			AMI_Debug.debugln(3, "unwrapData:: doing unwrap now");
		} catch (Exception exp) {}

		try {
			AMI_Crypto crypto = new AMI_Crypto();

			byte[] _key = privateKey.getEncoded();

			byte[] wrappedKey = info.getInput();

			crypto.ami_rsa_unwrap(wrappedKey, wrappedKey.length,
			    privateKey.getAlgorithm(), _key, _key.length,
			    "RSA");

			outputData = crypto.getOutputData();
		} catch (Exception e) {
			// e.printStackTrace();
			throw new AMI_Exception(e.toString());
		}

		try {
			AMI_Debug.debugln(3,
			    "In unwrapData complete..returning from server");
		} catch (Exception exp) {}

		return outputData;
	}

	/* Function to get the trusted certificates */
	public AMI_C_Certs[] getTrustedCertificates(AMI_EntryInfo info)
	throws AMI_Exception {
	    try {
		KeyStore ks = getInternalKeyStore(info);
		Vector certificates = new Vector();
		Enumeration enum = ks.aliases();
		while (enum.hasMoreElements()) {
			String alias = (String) enum.nextElement();
			if (ks.isKeyEntry(alias))
				continue;
			Certificate cert = ks.getCertificate(alias);
			AMI_C_Certs cCert = new AMI_C_Certs();
			cCert.setCertificate(cert);
			certificates.add(cCert);
		}
		AMI_C_Certs answer[] = new AMI_C_Certs[certificates.size()];
		for (int i = 0; i < answer.length; i++)
			answer[i] = (AMI_C_Certs) certificates.elementAt(i);
		return (answer);
	    } catch (Exception e) {
		// e.printStackTrace();
		return (null);
	    }
	}

	/* Function to change the password of KeyStore */
	public byte[] changeKeyStorePassword(String oldPasswd,
	String newPasswd, byte[] keystore) throws AMI_Exception {
	    try{
		ByteArrayInputStream is = new ByteArrayInputStream(
		    keystore);
		KeyStore okeyStore = KeyStore.getInstance("jks");
		okeyStore.load(is, oldPasswd.toCharArray());

		KeyStore nkeyStore = KeyStore.getInstance("jks");
		nkeyStore.load(null, null);
		
		Enumeration enum = okeyStore.aliases();
                while (enum.hasMoreElements()) {
		    String alias = (String) enum.nextElement();
	            if (okeyStore.isKeyEntry(alias)) {
			nkeyStore.setKeyEntry(alias, (PrivateKey)
			    (okeyStore.getKey(alias, oldPasswd.toCharArray())),
			    newPasswd.toCharArray(), 
			    okeyStore.getCertificateChain(alias));
	            } else {
	            	nkeyStore.setCertificateEntry(alias,
	                    okeyStore.getCertificate(alias));
	            }
	        }

		ByteArrayOutputStream os = new ByteArrayOutputStream();
		nkeyStore.store(os, newPasswd.toCharArray());
		return (os.toByteArray());
	    } catch (Exception e) {
		return (null);
	    }
	}


	protected void deleteKeyStore(AMI_Store store)
	    throws IOException, Exception {
		AMI_Debug.debugln(3,
		    "AMI_OperationsImpl:: Deleting key store  : " + store);

		_keyStores.removeElement(store);
		if (store.isPermanentKeyStore())
			savePermanentKey(null, store.getKeyStoreType(),
			    store.getEntryInfo());
	}

	protected void addKeyStore(KeyStore keyStore, String keyStoreType,
	    String password, AMI_EntryInfo entry, int permanent,
	    String rsaSignAlias, String rsaEncryptAlias, String dsaAlias)
	    throws Exception, IOException {
		AMI_Store store = new AMI_Store(keyStore, keyStoreType, 
		    password, entry, permanent, rsaSignAlias,
		    rsaEncryptAlias, dsaAlias);
		_keyStores.addElement(store);
		if ((permanent & AMI_Constants.AMI_PERMANENT) == 1)
			savePermanentKey(store, store.getKeyStoreType(), entry);
		AMI_Debug.debugln(3,
		    "AMI_OperationsImpl::Added key store  : " + store);
	}

	protected synchronized void savePermanentKey(AMI_Store store,
	    String keyStoreType, AMI_EntryInfo info)
	    throws AMI_Exception, Exception {
		AMI_Debug.debugln(3,
		    "AMI_OperationsImpl:: In savePermanentKey  : " + store);

		// Check if the directory and key store file exists 
		File file1 = new File(AMI_Constants.AMI_PERM_KEYS_DIR);
		Vector list = null;
		if (!file1.exists()) {
			AMI_Debug.debugln(3,
			    "AMI_KeyServImpl::Making perm keys directory");
			if (!file1.mkdirs()) {
				_msgFormatter.applyPattern(_messages.getString(
				    "AMI_Cmd.server.createDir"));
				Object[] args = { new String(
				    AMI_Constants.AMI_PERM_KEYS_DIR) };
				throw new AMI_Exception(
				    _msgFormatter.format(args));
			}
			
		}

		File file2 = new File(AMI_Constants.AMI_PERM_KEYS_FILENAME);
		if (file2.exists()) {
			// If it exists, read all entires from
			// the file into a list.
			AMI_Debug.debugln(3,
			    "AMI_KeyServImpl::Readin perm keys file");
			BufferedReader reader = new BufferedReader(
			    new InputStreamReader(new FileInputStream(file2)));
			list = readKeyStore(reader);
		} else
			list = new Vector();

		// Removed the item from the list
		for (int ii = 0; ii < list.size(); ii++) {
			AMI_Store storeInList = (AMI_Store)list.elementAt(ii);
		   	if ((storeInList.getEntryInfo().getUserId() ==
			    info.getUserId()) &&
			    (storeInList.getEntryInfo().getHostIP().compareTo(
			    info.getHostIP()) == 0) &&
			    (storeInList.getKeyStoreType().compareTo(
			    keyStoreType) == 0)) {
				list.removeElementAt(ii);
				break;
			}
		}

		// Write all remaining entries in the list to the file.
		DataOutputStream dos =  new DataOutputStream(
		    new FileOutputStream(file2));
		for (int ii = 0; ii < list.size(); ii++) {
			AMI_Store storeInList = (AMI_Store)list.elementAt(ii);
			writeKeyStore(dos, storeInList,
			    storeInList.getEntryInfo());
		}

		// If the passed in store is not null write to file.
		if (store != null) {
			writeKeyStore(dos, store, info);
		}
	}

	protected void writeKeyStore(DataOutputStream dos,
	    AMI_Store store, AMI_EntryInfo info) throws Exception {
		// Write the userid
		dos.writeChars(String.valueOf(info.getUserId()) + "\n");
		dos.writeChars(SEPERATOR + "\n");

		// Write the Key store type
		dos.writeChars(store.getKeyStoreType() + "\n");
		dos.writeChars(SEPERATOR + "\n");

		// Write the host IP address
		dos.writeChars(info.getHostIP() + "\n");
		dos.writeChars(SEPERATOR + "\n");

		// Write out the key store info
		PrintStream ps = new PrintStream(dos);
		store.write(ps);
		dos.writeChars(SEPERATOR + "\n");
	}

	protected Vector readKeyStore(BufferedReader reader) throws Exception {
		String line;
		StringBuffer buffer = new StringBuffer("");
		String hostIP;
		String userId;
		String keyStoreType;
		boolean readKeyData = false;
		byte[] store;
		Vector list = new Vector();

		userId = reader.readLine();
		while (userId != null) {
			readKeyData = false;
			buffer = new StringBuffer("");
			line = reader.readLine();
			if (line.indexOf(SEPERATOR) < 0)
				throw new AMI_Exception(_messages.getString(
				    "AMI_Cmd.server.readFile"));

			keyStoreType = reader.readLine();
			if (reader.readLine().indexOf(SEPERATOR) < 0)
				throw new AMI_Exception(_messages.getString(
				    "AMI_Cmd.server.readFile"));

			hostIP = reader.readLine();
			if (reader.readLine().indexOf(SEPERATOR) < 0)
				throw new AMI_Exception(_messages.getString(
				    "AMI_Cmd.server.readFile"));

			while (!readKeyData) {
				line = reader.readLine();
				if (line.indexOf(SEPERATOR) >= 0)
					readKeyData = true;
				else
					buffer.append(line);
			}

			BASE64Decoder decoder = new BASE64Decoder();
			store = decoder.decodeBuffer(buffer.toString());
			list.addElement(new AMI_Store(store));
			userId = reader.readLine();
		}
		return list;
	}

	protected AMI_Store findKeyStore(String keyStoreType,
	    AMI_EntryInfo info)
	    throws AMI_NoKeyStoreFoundException {
		for (int ii = 0; ii < _keyStores.size(); ii++) {
			AMI_Store store = (AMI_Store)
			    _keyStores.elementAt(ii);
			if (store.checkMatch(keyStoreType, info))
				return store;
		}
		try {
		AMI_Debug.debugln(3, "No key store found!");
		} catch (Exception e) {}
		_msgFormatter.applyPattern(_messages.getString(
		    "AMI_Cmd.server.nokeystore"));
		Object [] args = { new String(info.getHostIP()),
		    new String(String.valueOf(info.getUserId())),
		    new String(keyStoreType) };
		throw new AMI_NoKeyStoreFoundException(
		    _msgFormatter.format(args));
	}

	protected PrivateKey findPrivateKey(String keyStoreType,
	    String keyAlias, AMI_EntryInfo info, int keyUsage)
	    throws AMI_NoKeyStoreFoundException,
	    AMI_UnableToGetPrivateKeyException,	Exception {
		AMI_Store store = findKeyStore(keyStoreType, info);
		PrivateKey pkey = store.getPrivateKey(keyStoreType,
		    keyAlias, keyUsage);
		if (pkey == null) {
			_msgFormatter.applyPattern(_messages.getString(
			    "AMI_Cmd.server.noprivkey"));
			Object[] args = { new String(keyStoreType) };
			throw new AMI_UnableToGetPrivateKeyException(
			    _msgFormatter.format(args));
		}
		return pkey;
	} 

	protected void populateKeyStore(KeyStore dest, KeyStore source,
	    String password) throws Exception {
		Enumeration aliases = source.aliases();
		String alias = null;

		AMI_Debug.debugln(3, "populateKeyStore::copying");

		while (aliases.hasMoreElements()) {
			alias = (String) aliases.nextElement();		   
			if (source.isKeyEntry(alias)) {
				Key key = source.getKey(alias,
				    password.toCharArray());
				Certificate[] certs =
				    source.getCertificateChain(
				    alias);
				AMI_PrivateKey privKey = new AMI_PrivateKey(
				    alias, key.getAlgorithm());
				dest.setKeyEntry(alias, privKey, null, certs);
			} else if (source.isCertificateEntry(alias)) {
				dest.setCertificateEntry(alias,
				    source.getCertificate(alias));
			}
		}
	}

	static protected ResourceBundle _messages;
	static protected MessageFormat  _msgFormatter;
	static protected boolean _initialised;
	static protected Vector	_keyStores;
}


class AMI_Store {
	protected final byte[] xor_buf = {
	    (byte)'y', (byte)'a', (byte)'h', (byte)'y', (byte)'a', (byte)'a',
	    (byte)'b', (byte)'u', (byte)'a', (byte)'l', (byte)'s', (byte)'a',
	    (byte)'l', (byte)'q', (byte)'a', (byte)'n', (byte)'a', (byte)'r',
	    (byte)'a', (byte)'v', (byte)'i', (byte)'n', (byte)'d', (byte)'a',
	    (byte)'n', (byte)'r', (byte)'a', (byte)'n', (byte)'g', (byte)'a',
	    (byte)'n', (byte)'a', (byte)'t', (byte)'h', (byte)'a', (byte)'n',
	    (byte)'s', (byte)'a', (byte)'n', (byte)'g', (byte)'e', (byte)'e',
	    (byte)'t', (byte)'a', (byte)'v', (byte)'a', (byte)'r', (byte)'m',
	    (byte)'a', (byte)'y', (byte)'a', (byte)'h', (byte)'y', (byte)'a',
	    (byte)'a', (byte)'b', (byte)'u', (byte)'a', (byte)'l', (byte)'s',
	    (byte)'a', (byte)'l', (byte)'q', (byte)'a' };

	public AMI_Store(KeyStore keyStore, String keyStoreType,
	    String password, AMI_EntryInfo entry, int permanent,
	    String rsaSignAlias, String rsaEncryptAlias, String dsaAlias)
	    throws Exception {
		_entry = entry;
		_keyStoreType = keyStoreType;
		_keyStore = keyStore;
		_flags = permanent;

		// Store the keystore password Xor'ed to protect it.
		_password = XORBuffer(password.getBytes());

		_rsaSignAlias = rsaSignAlias;
		_rsaEncryptAlias = rsaEncryptAlias;
		_dsaAlias = dsaAlias;
	
	} 

	public AMI_Store(byte[] input) throws Exception {
		DerInputStream derInput;
		DerValue val  = new DerValue(input);

		if (val.tag != DerValue.tag_Sequence)
			throw new Exception("Invalid format ..");

		derInput = (new DerValue(input)).data;
		read(derInput);
	}

	public boolean checkMatch(String keyStoreType, AMI_EntryInfo info) {
		// Check the key store type 
		if (_keyStoreType.compareTo(keyStoreType) != 0)
			return false;

		// Check remaining attrs (id and ip)
		return (_entry.equals(info));
	}
	
	public PrivateKey getPrivateKey(String keyType, String keyAlias,
	    int keyUsage) throws AMI_UnableToGetPrivateKeyException, Exception {
		// Order of key:
		// 1. User supplied key alias.
		// 2. RSA/DSA default alias
		// 3. mykey
		// 4. mykeyRSA or mykeyDSA
		PrivateKey pkey = null;

		boolean tryDefault = false;
		boolean tryKeyDefault = false;
		if (keyAlias == null) {
			if ((keyType.equals("RSA")) &&
			    (keyUsage == AMI_Constants.SIGN))
				keyAlias = _rsaSignAlias;
			else if ((keyType.equals("RSA")) &&
			    (keyUsage == AMI_Constants.ENCRYPT))
				keyAlias = _rsaEncryptAlias;
			else if (keyType.equals("DSA"))
				keyAlias = _dsaAlias;
		}
		if (keyAlias == null) {
			tryDefault = true;
			keyAlias = AMI_Constants.DEFAULT_ALIAS;
		} else if (keyAlias.equalsIgnoreCase(
		    AMI_Constants.DEFAULT_ALIAS)) {
			tryKeyDefault = true;
		}

		try {
			AMI_Debug.debugln(3,
			    "getPrivateKey::looking for key with alias = " +
			    keyAlias);
			if ((pkey = (PrivateKey) (_keyStore.getKey(keyAlias,
			    (new String(XORBuffer(_password))).toCharArray())))
			    == null) {
				// Check if defaultKeyAlias can be used
				if (tryDefault) {
					if (keyType.equals("RSA"))
						keyAlias =
					    AMI_Constants.DEFAULT_RSA_ALIAS;
					else if (keyType.equals("DSA"))
						keyAlias =
					    AMI_Constants.DEFAULT_DSA_ALIAS;
					AMI_Debug.debugln(3,
					    "getPrivateKey::looking for " +
					    "key with alias:" +
					    keyAlias);
				} else if (tryKeyDefault) {
					if ((keyType.equals("RSA")) &&
					    (keyUsage == AMI_Constants.SIGN))
						keyAlias = _rsaSignAlias;
					else if ((keyType.equals("RSA")) &&
					    (keyUsage == AMI_Constants.ENCRYPT))
						keyAlias = _rsaEncryptAlias;
					else if (keyType.equals("DSA"))
						keyAlias = _dsaAlias;
				}
				if (keyAlias != null) {
					pkey = (PrivateKey)(_keyStore.getKey(
					    keyAlias,
					    (new String(XORBuffer(
					    _password))).toCharArray()));
				}
			}
			if (pkey != null)
				AMI_Debug.debugln(3,
				    "getPrivateKey::found key:" + pkey);
			return pkey;
		} catch (Exception e) {
			throw new AMI_UnableToGetPrivateKeyException(
			    e.getMessage());
		}
	}

	public String getUserName() {
		return _entry.getUserName();
	}

	public KeyStore getKeyStore() {
		return _keyStore;
	}

	public String getKeyStoreType() {
		return _keyStoreType;
	}

	public AMI_EntryInfo getEntryInfo() {
		return _entry;
	}

	public String getPassword() {
		return (new String(XORBuffer(_password)));
	}

	public boolean isPermanentKeyStore() {
		if ((_flags & AMI_Constants.AMI_PERMANENT) == 1)
			return true;
		else
			return false;
	}

	public void write(PrintStream out)  throws Exception {
		DerOutputStream derOut = new DerOutputStream();
		DerOutputStream tmp = new DerOutputStream();

		ByteArrayOutputStream baos = new ByteArrayOutputStream();

		// Write the keystore
		_keyStore.store(baos, "".toCharArray());	
		derOut.putBitString(baos.toByteArray());

		// Write other AMI_Store data
		derOut.putBitString(_keyStoreType.getBytes());
		derOut.putBitString(_password);
		derOut.putInteger(new BigInt(_flags));

		// Write default aliases
		derOut.putBitString((_rsaSignAlias == null) ?
		    (AMI_Constants.DEFAULT_ALIAS).getBytes() :
		    _rsaSignAlias.getBytes());
		derOut.putBitString((_rsaEncryptAlias == null) ?
		    (AMI_Constants.DEFAULT_ALIAS).getBytes() :
		    _rsaEncryptAlias.getBytes());
		derOut.putBitString((_dsaAlias == null) ?
		    (AMI_Constants.DEFAULT_ALIAS).getBytes() :
		    _dsaAlias.getBytes());

		// Write AMI_Entry data
		_entry.write(derOut);
		tmp.write(DerValue.tag_Sequence, derOut);

		// Base64 encode the output
		BASE64Encoder encoder = new BASE64Encoder();
	 	encoder.encodeBuffer(tmp.toByteArray(), out);
	}

	protected void read(DerInputStream in)  throws Exception {
		// Load the keystore
		byte[] bitStr = null;

		bitStr = in.getBitString();
		_keyStore = KeyStore.getInstance("jks");
		_keyStore.load(new ByteArrayInputStream(bitStr), null);

		// Read other AMI_Store data
		bitStr = in.getBitString();
		_keyStoreType = new String(bitStr);
		_password = in.getBitString();
		_flags = in.getInteger().toInt();

		// Read default aliases
		_rsaSignAlias = new String(in.getBitString());
		_rsaEncryptAlias = new String(in.getBitString());
		_dsaAlias = new String(in.getBitString());

		// Read AMI_Entry data
		_entry = new AMI_EntryInfo(in);
	}


	protected byte[] XORBuffer(byte[] input) {
		byte[] output = new byte[input.length];
		for (int xor_count = 0; xor_count < input.length; xor_count++) {
			output[xor_count] = (byte)
			    (input[xor_count] ^	xor_buf[xor_count %
			    xor_buf.length]);
		}
		return output;
	}

	public String toString() {
		StringBuffer sb = new StringBuffer("[ AMI_Store : \n");

		sb.append("\tKeyStoreType = " + _keyStoreType + "\n");
		sb.append("\tFlags = " + _flags + "\n");
		sb.append(_entry.toString() + " ]");

		return (sb.toString());
	}

	protected KeyStore	_keyStore;  // Password protected keystore
	protected String	_keyStoreType;
	protected byte[]	_password;  // Xor'ed keystore password
	protected AMI_EntryInfo _entry;
	protected int		_flags;

	protected String	_rsaSignAlias = null;
	protected String	_rsaEncryptAlias = null;
	protected String	_dsaAlias = null;
}
