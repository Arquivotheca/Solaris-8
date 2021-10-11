/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_UserProfile.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.keymgnt;

import java.lang.*;
import java.util.*;
import java.io.*;
import javax.naming.*;
import javax.naming.directory.*;

public class AMI_UserProfile extends Object implements Serializable {
	protected static final int VERSION = 1;

	// Supported operations
	protected static final String RSA_SIGN_KEY = "defaultRSAsignaturekey";
	protected static final String RSA_ENCRYPT_KEY =
	    "defaultRSAencryptionkey";
	protected static final String DSA_KEY = "defaultDSAkey";
	protected static final String DH_KEY = "defaultDHkey";
	protected static final String KEYSTORE_FILE = "keystorefile";
	protected static final String CERT_FILE = "certx509file";
	protected static final String CACERT_FILE = "cacertx509File";
	protected static final String CERTCHAIN_FILE = "certchainx509file";
	protected static final String CERTREQ_FILE = "certreqfile";
	protected static final String BACKUPCERT_FILE =
	    "backupcertreqcertsfile";
	protected static final String NAME_DN_ALIAS = "namednalias";

	// Key Types
	protected static final String KEY_TYPE_RSA = "RSA";
	protected static final String KEY_TYPE_DSA = "DSA";

	protected String keyType;
	protected String keyAlias;
	protected Hashtable properties;
	protected byte[] signature;

	public AMI_UserProfile() {
		keyType = new String("");
		keyAlias = new String("");
		properties = new Hashtable();
		signature = null;
	}

	public AMI_UserProfile(byte[] serializedObject) throws Exception {
		DataInputStream dis = new DataInputStream(
		   new ByteArrayInputStream(serializedObject));

		int version = dis.readInt();
		if (version != VERSION) {
			throw new AMI_KeyMgntException(
			    "Invalid user profile object");
		}

		keyType = dis.readUTF();
		keyAlias = dis.readUTF();

		int count = dis.readInt();
		properties = new Hashtable();
		String key, value;
		for (int i = 0; i < count; i++) {
			key = dis.readUTF();
			value = dis.readUTF();
			properties.put(key, value);
		}

		int signLen = dis.readInt();
		if (signLen == 0) {
			signature = null;
		} else {
			signature = new byte[signLen];
			dis.readFully(signature);
		}

		// %%% Verify the signature
	}

	public byte[] toByteArray() throws Exception {
		ByteArrayOutputStream baos = new ByteArrayOutputStream();
		DataOutputStream dos = new DataOutputStream(baos);

		dos.writeInt(VERSION);

		dos.writeUTF(keyType);
		dos.writeUTF(keyAlias);

		dos.writeInt(properties.size());
		Enumeration enum = properties.keys();
		String key, value;
		while (enum.hasMoreElements()) {
			key = (String) enum.nextElement();
			value = (String) properties.get(key);
			dos.writeUTF(key);
			dos.writeUTF(value);
		}

		// %%% Actually must sign the data

		if (signature == null)
			dos.writeInt(0);
		else {
			dos.writeInt(signature.length);
			dos.write(signature);
		}
		return (baos.toByteArray());
	}
		
	public void clear() {
		properties.clear();
	}

	public void merge(AMI_UserProfile profile) {
		properties.putAll(profile.properties);
	}

	public String getProperty(String id) {
		return ((String) properties.get(id));
	}

	public void setIndexNameAttribute(String id, String value) {
		properties.put(id, value);
	}

	public String getIndexNameAttribute(String id) {
		return ((String) properties.get(id));
	}

	public void removeIndexNameAttribute(String id) {
		properties.remove(id);
	}

	public void setRSAsignkey(String alias) {
		properties.put(RSA_SIGN_KEY, alias);
	}

	public String getRSAsignkey() {
		return ((String) properties.get(RSA_SIGN_KEY));
	}

	public void removeRSAsignkey() {
		properties.remove(RSA_SIGN_KEY);
	}

	public void setRSAencryptkey(String alias) {
		properties.put(RSA_ENCRYPT_KEY, alias);
	}

	public String getRSAencryptkey() {
		return ((String) properties.get(RSA_ENCRYPT_KEY));
	}

	public void removeRSAencryptkey() {
		properties.remove(RSA_ENCRYPT_KEY);
	}

	public void setDSAkey(String alias) {
		properties.put(DSA_KEY, alias);
	}

	public String getDSAkey() {
		return ((String) properties.get(DSA_KEY));
	}

	public void removeDSAkey() {
		properties.remove(DSA_KEY);
	}

	public void setDHkey(String alias) {
		properties.put(DH_KEY, alias);
	}

	public String getDHkey() {
		return ((String) properties.get(DH_KEY));
	}

	public void removeDHkey() {
		properties.remove(DH_KEY);
	}

	public void setKeyStoreFile(String path) {
		properties.put(KEYSTORE_FILE, path);
	}

	public String getKeyStoreFile() {
		return ((String) properties.get(KEYSTORE_FILE));
	}

	public void removeKeyStoreFile() {
		properties.remove(KEYSTORE_FILE);
	}

	public void setCertificateFile(String path) {
		properties.put(CERT_FILE, path);
	}

	public String getCertificateFile() {
		return ((String) properties.get(CERT_FILE));
	}

	public void removeCertificateFile() {
		properties.remove(CERT_FILE);
	}

	public void setCertificateChainFile(String path) {
		properties.put(CERTCHAIN_FILE, path);
	}

	public String getCertificateChainFile() {
		return ((String) properties.get(CERTCHAIN_FILE));
	}

	public void removeCertificateChainFile() {
		properties.remove(CERTCHAIN_FILE);
	}

	public void setCaCertificateFile(String path) {
		properties.put(CACERT_FILE, path);
	}

	public String getCaCertificateFile() {
		return ((String) properties.get(CACERT_FILE));
	}

	public void removeCaCertificateFile() {
		properties.remove(CACERT_FILE);
	}

	public void setCertReqFile(String path) {
		properties.put(CERTREQ_FILE, path);
	}

	public String getCertReqFile() {
		return ((String) properties.get(CERTREQ_FILE));
	}

	public void removeCertReqFile() {
		properties.remove(CERTREQ_FILE);
	}

	public void setBackupCertificateFile(String path) {
		properties.put(BACKUPCERT_FILE, path);
	}

	public String getBackupCertificateFile() {
		return ((String) properties.get(BACKUPCERT_FILE));
	}

	public void removeBackupCertificateFile() {
		properties.remove(BACKUPCERT_FILE);
	}

	public void setNameDNAlias(String alias) {
		int index = alias.indexOf('=');
		if (index == -1)
			return;
		String key = alias.substring(0, index);
		String value = alias.substring(index+1);

		key = new String(NAME_DN_ALIAS + key.trim());
		properties.put(key.trim().toLowerCase(),
		    value.trim().toLowerCase());
	}

	public void removeNameDNAlias(String alias) {
		int index;
		String key;
		String[] keys = getNameDNAlias(alias);
		for (int i = 0; ((keys != null) && (i < keys.length)); i++) {
			index = keys[i].indexOf('=');
			if (index != -1) {
				key = keys[i].substring(0, index);
				key = new String(NAME_DN_ALIAS + key);
				properties.remove(key.toLowerCase());
			}
		}
	}

	public String[] getNameDNAlias(String alias) {
		String answer[] = null;
		String key, value;
		if (alias != null) {
			int index = alias.indexOf('=');
			if (index != -1)
				key = alias.substring(0, index);
			else
				key = alias;
			String keyIndex = new String(NAME_DN_ALIAS +
			    key.trim());
			value = (String) properties.get(
			    keyIndex.toLowerCase());
			if (value == null)
				return (null);
			answer = new String[1];
			answer[0] = new String(key + "=" + value);
			return (answer);
		}

		Enumeration enum = properties.keys();
		Vector keys = new Vector();
		while (enum.hasMoreElements()) {
			key = (String) enum.nextElement();
			if (key.startsWith(NAME_DN_ALIAS))
				keys.add(key);
		}
		if (keys.size() == 0)
			return (null);

		enum = keys.elements();
		Vector values = new Vector();
		while (enum.hasMoreElements()) {
			key = (String) enum.nextElement();
			value = (String) properties.get(key);
			key = key.substring(NAME_DN_ALIAS.length());
			values.add(key + "=" + value);
		}

		if (values.size() == 0)
			return (null);
		answer = new String[values.size()];
		enum = values.elements();
		int i = 0;
		while (enum.hasMoreElements())
			answer[i++] = (String) enum.nextElement();
		return (answer);
	}

	public void removeNonIndexNameAttributes() {
		removeRSAsignkey();
		removeRSAencryptkey();
		removeDSAkey();
		removeDHkey();
		removeKeyStoreFile();
		removeCertificateFile();
		removeCertificateChainFile();
		removeCaCertificateFile();
		removeCertReqFile();
		removeBackupCertificateFile();
	}

	public String toString() {
		String answer = new String("AMI User Profile Object\n");
		answer += properties.toString();
		return (answer);
	}
}
