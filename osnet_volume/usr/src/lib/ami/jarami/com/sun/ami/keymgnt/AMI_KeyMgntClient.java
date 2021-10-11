/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_KeyMgntClient.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.keymgnt;

import java.io.*;
import java.net.*;
import java.util.*;
import java.security.*;
import java.security.cert.*;

import javax.naming.*;
import javax.naming.directory.*;

import com.sun.ami.*;
import com.sun.ami.common.*;
import com.sun.ami.ca.*;
import com.sun.ami.keygen.AMI_VirtualHost;

public class AMI_KeyMgntClient extends Object {
	/**
	 * Properties file name
	 */
	protected static final String AMI_PROPERTIES_FILE =
	    "/etc/ami/ami.properties";

	/**
	 * Properties to hold AMI configuration, schema and definitions
	 */
	protected static AMI_KeyMgntSchema amiKeyMgntSchema = null;
	protected static Properties amiProperties = null;

	/**
	 * Hashtable to cache the AMI_KeyMgnt Objects
	 */
	protected static Hashtable amiKeyMgntObjects = new Hashtable();

	/**
	 * Method to load the AMI configuration from /etc/ami/ami.conf
	 */
	// %%% This method cannot have any debug or log messages
	protected static synchronized void loadAmiProperties()
	    throws AMI_KeyMgntException, IOException {
		// Check if the properties have been initialized
		if (amiProperties != null)
			return;

		amiProperties = new Properties();
		File amiPropFile = new File(AMI_PROPERTIES_FILE);
		if (amiPropFile.exists() && amiPropFile.canRead()) {
			try {
				Properties originalProps = new Properties();
				originalProps.load(new
				    FileInputStream(amiPropFile));
				Enumeration enum =
				    originalProps.propertyNames();
				String key, value;
				while (enum.hasMoreElements()) {
					key = (String) enum.nextElement();
					value = (String)
					    originalProps.getProperty(key);
					amiProperties.put(
					    key.toLowerCase(),
					    originalProps.getProperty(
					    key));
				}
			} catch (IOException ioe) {
				throw new AMI_KeyMgntException(
				    "Could not load AMI property file: " +
				    AMI_PROPERTIES_FILE + "\n" +
				    ioe.getLocalizedMessage());
			}
		} else {
			throw new AMI_KeyMgntException(
			    "AMI properties file not present " +
			    " or unable to read: " + AMI_PROPERTIES_FILE);
		}
	}

	/**
	 * Method to get a property from the AMI property file
	 */
	public static String getProperty(String key) 
            throws AMI_KeyMgntException, IOException {

		if (amiProperties == null)
			loadAmiProperties();
		return (amiProperties.getProperty(key.toLowerCase()));
	}

	/**
	 * Method to set a property for AMI_KeyMgnt Environment
	 */
	public static void setProperty(String key, String value)
	    throws AMI_KeyMgntException, IOException {
		try {
			if (amiProperties == null)
				loadAmiProperties();
		} catch (Exception e) {
			AMI_Debug.debugln(1,
			    "Unable to set AMI_KeyMgnt environment: " +
			    key + "\n" + e.getLocalizedMessage());
			return;
		}
		amiProperties.put(key.toLowerCase(), value);

		// Remove all AMI_KeyMgnt objects from cache
		amiKeyMgntObjects.clear();
	}

	/**
	 * Method to get AMI_KeyMgnt Object based on the attribute ID
	 */
	public static AMI_KeyMgnt getAMIKeyMgntInstance(String attrType,
	    String amiType) throws AMI_KeyMgntException, IOException {
		AMI_Debug.debugln(3,
		    "Determining name service backend for: " + attrType);

		// Get the name service backend
		String nsBackendProperty = new String("ami.keymgnt.ns." +
		    attrType.toLowerCase());
		String nsBackend = getProperty(nsBackendProperty);
		if (nsBackend == null) {
			AMI_Debug.debugln(1,
			    "Error in determining name service backend for: "
			    + attrType);
			throw new AMI_KeyMgntException(
			    "Error in determining name service backend for: "
			    + attrType);
		}
		return (getAMIKeyMgntForBackend(nsBackend, amiType));
	}

	/**
	 * Method to get AMI_KeyMgnt Object for the specified backend
	 */
	protected static AMI_KeyMgnt getAMIKeyMgntForBackend(String nsBackend,
	    String amiType) throws AMI_KeyMgntException, IOException {
		AMI_Debug.debugln(3,
		    "Obtain AMI_KeyMgnt object for backend: "
		    + nsBackend + " and AMI type: " + amiType);

		// Check in the cache if the AMI_KeyMgntObject exists
		AMI_KeyMgnt amiKeyMgnt = null;
		nsBackend = nsBackend.toLowerCase();
		String amiKeyMgntIndex = new String(nsBackend + amiType);
		if ((amiKeyMgnt = (AMI_KeyMgnt) amiKeyMgntObjects.get(
		    amiKeyMgntIndex)) != null) {
			AMI_Debug.debugln(3, "AMI_KeyMgnt Object for backend: "
			    + nsBackend + " and AMI type: " + amiType
			    + " found in cache");
			return (amiKeyMgnt);
		}
		return (instantiateAMIKeyMgntForBackend(nsBackend, amiType));
	}

	/**
	 * Method to create AMI_KeyMgnt Object for the specified backend
	 */
	protected static synchronized AMI_KeyMgnt
	    instantiateAMIKeyMgntForBackend(String nsBackend,
	    String amiType) throws AMI_KeyMgntException, IOException {
		// Since nis and nisplus defaults to FNS
		if (nsBackend.startsWith("nis"))
			nsBackend = new String("fns");

		AMI_Debug.debugln(3,
		    "Instantiating AMI_KeyMgnt Object for backend: "
		    + nsBackend + " and AMI type: " + amiType);

		// Check if present in cache
		AMI_KeyMgnt amiKeyMgnt = null;
		String amiKeyMgntIndex = new String(nsBackend + amiType);
		if ((amiKeyMgnt = (AMI_KeyMgnt) amiKeyMgntObjects.get(
		    amiKeyMgntIndex)) != null) {
			AMI_Debug.debugln(3, "AMI_KeyMgnt Object for backend: "
			    + nsBackend + " and AMI type: " + amiType
			    + " found in cache");
			return (amiKeyMgnt);
		}

		// Set appropriate property for Directory root
		String nsRoot = getProperty("ami.keymgnt." + nsBackend +
		    ".ami." + amiType + ".prefix");
		if (nsRoot != null) {
			AMI_Debug.debugln(3, "Directory root set to: " +
			    nsRoot);
			amiProperties.setProperty("ami.keymgnt." +
			    nsBackend + ".ami.directoryRoot",
			    normalizeDN(nsRoot));
		}

		// Set appropriate property for additional oc
		String addoc = getProperty("ami.keymgnt." + nsBackend +
		    ".ami." + amiType + ".addoc");
		if (addoc != null) {
			AMI_Debug.debugln(3, "Additional objectclass are: " +
			    addoc);
			amiProperties.setProperty("ami.keymgnt." + nsBackend +
			    ".ami.addoc", addoc);
		}

		// Set appropriate property for additional attrs
		String addattr = getProperty("ami.keymgnt." + nsBackend +
		    ".ami." + amiType + ".addattr");
		if (addattr != null) {
			AMI_Debug.debugln(3, "Additional attributes are: " +
			    addattr);
			amiProperties.setProperty("ami.keymgnt." + nsBackend +
			    ".ami.addattr", addattr);
		}

		// Set appropriate property for indexName
		String indexName = getProperty("ami.keymgnt." + nsBackend +
		    ".ami." + amiType + ".indexname");
		if (indexName == null) {
			AMI_Debug.debugln(3, "Index name property not set: "
			    + "ami.keymgnt." + nsBackend + ".ami." + amiType
			    + ".indexname");
			throw new AMI_KeyMgntException(
			    "IndexName property not set: " + "ami.keymgnt."
			    + nsBackend + ".ami." + amiType + ".indexname");
		}
		amiProperties.setProperty("ami.keymgnt." +
		    nsBackend + ".ami.indexname", indexName);

		// Create the schema, if not present
		if (amiKeyMgntSchema == null)
			amiKeyMgntSchema = new AMI_KeyMgntSchema(
			    "AMI", "files", amiProperties);

		// Instantiate AMI_KeyMgntObject and add to cache
		amiKeyMgnt = AMI_KeyMgnt.getInstance(
		    nsBackend, amiKeyMgntSchema, amiProperties);
		amiKeyMgntObjects.put(amiKeyMgntIndex,
		    amiKeyMgnt);
		return (amiKeyMgnt);
	}

	/**
	 * Method to get the indexName for the attribute type and ami type
	 */
	protected static String getIndexName(String searchName, String attrType,
	    String amiType) throws Exception {
                // Construct the AMI_KeyMgnt object
                AMI_KeyMgnt ami = getAMIKeyMgntInstance(attrType, amiType);

                // Construct attributeset, based on searchName syntax
                Attribute searchAttribute = null;
                if (searchName.indexOf('=') != -1)
                        searchAttribute = new BasicAttribute(
                            "namedn", normalizeDN(searchName));
                // else if ((searchName.indexOf('@') != -1) ||
		//    (searchName.indexOf('.') != -1))
                //        searchAttribute = new BasicAttribute(
                //            "namedns", searchName.toLowerCase());
		else if (amiType.equalsIgnoreCase(
		    AMI_Constants.AMI_USER_OBJECT))
			searchAttribute = new BasicAttribute(
			    "nameuser", searchName.toLowerCase());
		else if (amiType.equalsIgnoreCase(
		    AMI_Constants.AMI_HOST_OBJECT)) {
			// Make sure seachName is an IP address
			if (searchName.equalsIgnoreCase("root"))
				searchName = AMI_VirtualHost.getHostIP();
			searchAttribute = new BasicAttribute("namehost",
			    InetAddress.getByName(searchName).getHostAddress());
		} else if (amiType.equalsIgnoreCase(
		    AMI_Constants.AMI_APPLICATION_OBJECT))
			searchAttribute = new BasicAttribute(
			    "nameapplication", searchName.toLowerCase());

                // Perform the AMI search operation
		Enumeration enum = null;
		try {
			enum = ami.search(searchAttribute);
		} catch (AMI_KeyMgntCommunicationException ne) {
			amiKeyMgntObjects.clear();
			ami = getAMIKeyMgntInstance(attrType, amiType);
			enum = ami.search(searchAttribute);
		}

                if (enum == null) {
                        // Object is not present
                        return (null);
                }

                // Get the user's real "key" to index them
                String nameIndex = null;
                if (enum.hasMoreElements()) {
                        nameIndex = (String) enum.nextElement();
                        if (enum.hasMoreElements()) {
                                throw new AMI_KeyMgntException(
                                    "Multiple DirObjects having " +
				    "same login Name");
                        }
                }
		AMI_Debug.debugln(3, "Index name for " + searchName +
		    " is " + nameIndex);
                return (nameIndex);
        }

	/**
	 * Method to get an Attribute from directory service
	 */
	protected static Attribute getAttribute(String name, String attrType,
	    String amiType) throws Exception {
		// Debug information
		AMI_Debug.debugln(3, "Get attribute: " + attrType +
		    " for AMI Object: " + name);
		attrType = attrType.toLowerCase();
		AMI_KeyMgnt amiKeyMgnt = getAMIKeyMgntInstance(attrType,
		    amiType);

		// Method that gets the indexName from syntax for "name"
		String indexName = getIndexName(name, attrType.toLowerCase(),
		    amiType);
		if (indexName == null)
			// Object does not exists, hence return NULL
			return (null);

		// Get the Attribute
		return (amiKeyMgnt.getAttribute(indexName,
		    attrType.toLowerCase()));
	}

	/**
	 * Method to add an Attribute in directory service
	 */
	protected static void addAttribute(AMI_KeyMgntService service,
	    String name, String attrID, Object attrValue, String amiType)
	    throws Exception {
		// Debug information
		AMI_Debug.debugln(3, "Add attribute: " + attrID +
		    " for AMI Object: " + name);
		attrID = attrID.toLowerCase();
		AMI_KeyMgnt amiKeyMgnt = getAMIKeyMgntInstance(attrID, amiType);

		// Method that gets the indexName from syntax for "name"

		String indexName = getIndexName(name, attrID, amiType);

		if (indexName == null) {
			// Object not present, create the object
			createAmiObject(service.getNameLogin(),
			    service.getNameDN(), service.getNameDNS(),
			    amiType, amiKeyMgnt);
			indexName = getIndexName(name, attrID, amiType);
		}
		if (indexName == null)
			throw new AMI_KeyMgntException(
			    "Index name not found for name: " + name);

		// Add the new attribute
		Attributes addAttrs = new BasicAttributes(attrID, attrValue);

		amiKeyMgnt.addAttributes(indexName, addAttrs);
	}

	/**
	 * Method to delete an Attribute in directory service
	 */
	protected static void deleteAttribute(String name, String attrID,
	    Object attrValue, String amiType) throws Exception {
		// Debug information
		AMI_Debug.debugln(3, "Delete attribute: " + attrID +
		    " for AMI Object: " + name);
		attrID = attrID.toLowerCase();
		AMI_KeyMgnt amiKeyMgnt = getAMIKeyMgntInstance(attrID, amiType);

		// Method that gets the indexName from syntax for "name"
		String indexName = getIndexName(name, attrID, amiType);
		if (indexName == null) {
			// Object not present, hence success
			return;
		}

		// Delete the attribute
		Attributes delAttrs = new BasicAttributes();
		if (attrValue == null)
			delAttrs.put(new BasicAttribute(attrID));
		else
			delAttrs.put(new BasicAttribute(
			    attrID, attrValue));
		amiKeyMgnt.deleteAttributes(indexName, delAttrs);
	}

	/**
	 * Method to delete and then to add the given attribute
	 */
	protected static void deleteAndAddAttribute(AMI_KeyMgntService service,
	    String name, String attrID, Object attrValue, String amiType)
	    throws Exception {
		deleteAttribute(name, attrID, null, amiType);
		if (attrValue != null)
			addAttribute(service, name, attrID, attrValue, amiType);
	}

	/**
	 * Method to get the specified KeyStore
	 */
	public static KeyStore getKeyStore(String name, String key_type,
	    String keypass, String amiType) throws Exception {
		// Debug information
		AMI_Debug.debugln(3, "Get " + key_type +
		    " keystore for AMI Object: " + name);
		Attribute keystoreAttr = getAttribute(name, key_type, amiType);
		if (keystoreAttr == null)
			return (null);

		// Get the KeyStore
		Object obj = keystoreAttr.get();
		byte[] encoded_keystore;
		if (obj.getClass().getName().equals(
		    "java.lang.String"))
			encoded_keystore = ((String) obj).getBytes();
		else
			encoded_keystore = (byte[]) obj;
		ByteArrayInputStream byte_istream =
		    new ByteArrayInputStream(encoded_keystore);
		KeyStore keyStore = KeyStore.getInstance("jks");
		keyStore.load(byte_istream, keypass.toCharArray());
		return (keyStore);
	}
		
	/**
	 * Method to set the specified KeyStore
	 */
	public static void setKeyStore(AMI_KeyMgntService service,
	    String name, String key_type, KeyStore ks,
	    String keypass, String amiType) throws Exception {
		// Debug information
		AMI_Debug.debugln(3, "Set " + key_type +
		    " keystore for AMI Object: " + name);
		// Check if keystore is null
		if (ks == null)
			deleteKeyStore(name, key_type, amiType);

		// Get the keystore byte array
		ByteArrayOutputStream byte_ostream =
		    new ByteArrayOutputStream(2048);
		ks.store(byte_ostream, keypass.toCharArray());

		// Store in directory
		deleteAndAddAttribute(service, name, key_type,
		    (Object) byte_ostream.toByteArray(), amiType);
	}

	/**
	 * Method to delete the specified KeyStore
	 */
	public static void deleteKeyStore(String name, String key_type,
	    String amiType) throws Exception {
		// Debug information
		AMI_Debug.debugln(3, "Delete " + key_type +
		    " keystore for AMI Object: " + name);
		deleteAttribute(name, key_type, null, amiType);
	}

	/**
	 * Method to get RSA KeyStore
	 */
	public static KeyStore getRSAKeyStore(String name, String keypass,
	    String amiType) throws Exception {
		return (getKeyStore(name, "keystoreRSA", keypass, amiType));
	}

	/**
	 * Method to set RSA KeyStore
	 */
	public static void setRSAKeyStore(AMI_KeyMgntService service,
	    String name, KeyStore ks, String keypass, String amiType)
	    throws Exception {
		setKeyStore(service, name, "keystoreRSA", ks, keypass, amiType);
	}

	/**
	 * Method to delete RSA KeyStore
	 */
	public static void deleteRSAKeyStore(String name, String amiType)
	    throws Exception {
		deleteKeyStore(name, "keystoreRSA", amiType);
	}

	/**
	 * Method to get DSA KeyStore
	 */
	public static KeyStore getDSAKeyStore(String name, String keypass,
	    String amiType) throws Exception {
		return (getKeyStore(name, "keystoreDSA", keypass, amiType));
	}

	/**
	 * Method to set DSA KeyStore
	 */
	public static void setDSAKeyStore(AMI_KeyMgntService service,
	    String name, KeyStore ks, String keypass, String amiType)
	    throws Exception {
		setKeyStore(service, name, "keystoreDSA", ks, keypass, amiType);
	}

	/**
	 * Method to delete DSA KeyStore
	 */
	public static void deleteDSAKeyStore(String name, String amiType)
	    throws Exception {
		deleteKeyStore(name, "keystoreDSA", amiType);
	}

	/**
	 * Method to get DH KeyStore
	 */
	public static KeyStore getDHKeyStore(String name, String keypass,
	    String amiType) throws Exception {
		return (getKeyStore(name, "keystoreDH", keypass, amiType));
	}

	/**
	 * Method to set DH KeyStore
	 */
	public static void setDHKeyStore(AMI_KeyMgntService service,
	    String name, KeyStore ks, String keypass, String amiType)
	    throws Exception {
		setKeyStore(service, name, "keystoreDH", ks, keypass, amiType);
	}

	/**
	 * Method to delete DH KeyStore
	 */
	public static void deleteDHKeyStore(String name, String amiType)
	    throws Exception {
		deleteKeyStore(name, "keystoreDH", amiType);
	}

	/**
	 * Method to join two keystores
	 */
	protected static void appendKeyStore(String type, KeyStore answer,
	    KeyStore subset, String keypass) throws Exception {
		String alias;
		Key privateKey;
		java.security.cert.Certificate[] certChain;
		java.security.cert.Certificate certificate;
		Enumeration aliases = subset.aliases();
		boolean appendAlias;
		while (aliases.hasMoreElements()) {
			alias = (String) aliases.nextElement();
			if (answer.containsAlias(alias))
				appendAlias = true;
			else
				appendAlias = false;
			if (subset.isCertificateEntry(alias)) {
				certificate = subset.getCertificate(alias);
				answer.setCertificateEntry(appendAlias ?
				    alias+type : alias, certificate);
			} else {
				privateKey = subset.getKey(alias,
				    keypass.toCharArray());
				certChain = subset.getCertificateChain(
				    alias);
				answer.setKeyEntry(appendAlias ?
				    alias+type : alias, privateKey,
				    keypass.toCharArray(), certChain);
			}
		}
	}

	/**
	 * Method to get All the KeyStore
	 */
	public static KeyStore getKeyStore(String name, String keypass,
	    String amiType) throws Exception {
		// Debug information
		AMI_Debug.debugln(3,
		    "Get ALL keystore for AMI Object: " + name);

		String nstype, rsaattrid, dsaattrid, dhattrid;
		KeyStore rsa, dsa, dh, answer = null;

		// First get RSA keystore
		nstype = getProperty("ami.keymgnt.ns.keystorersa");
		rsaattrid = getProperty("ami.keymgnt." + nstype +
		    ".ami.keystorersa");
		if ((rsa = getRSAKeyStore(name, keypass, amiType)) != null)
			answer = rsa;

		dsaattrid = getProperty("ami.keymgnt." + nstype +
		    ".ami.keystoredsa");
		if ((!rsaattrid.equalsIgnoreCase(dsaattrid)) &&
		    ((dsa = getDSAKeyStore(name, keypass, amiType)) != null)) {
			if (answer == null)
				answer = dsa;
			else
				appendKeyStore("DSA", answer, dsa, keypass);
		}

		dhattrid = getProperty("ami.keymgnt." + nstype +
		    ".ami.keystoredh");
		if ((!rsaattrid.equalsIgnoreCase(dhattrid)) &&
		    (!dsaattrid.equalsIgnoreCase(dhattrid)) &&
		    (dh = getDHKeyStore(name, keypass, amiType)) != null) {
			if (answer == null)
				answer = dh;
			else
				appendKeyStore("DH", answer, dh, keypass);
		}
		return (answer);
	}

	public static void deleteKeyStore(String name, String amiType)
	    throws Exception {
		AMI_Debug.debugln(3,
		    "Delete ALL keystores for AMI Object: " + name);
		String nstype, rsaattrid, dsaattrid, dhattrid;

		// First delete the RSA keystore
		nstype = getProperty("ami.keymgnt.ns.keystorersa");
		rsaattrid = getProperty("ami.keymgnt." + nstype +
		    ".ami.keystorersa");
		deleteRSAKeyStore(name, amiType);

		// Delete DSA keystore, if different from RSA
		dsaattrid = getProperty("ami.keymgnt." + nstype +
		    ".ami.keystoredsa");
		if (!rsaattrid.equalsIgnoreCase(dsaattrid))
			deleteDSAKeyStore(name, amiType);

		// Delete DH Keystore
		dhattrid = getProperty("ami.keymgnt." + nstype +
		    ".ami.keystoredh");
		if (!rsaattrid.equalsIgnoreCase(dhattrid) &&
		    !dsaattrid.equalsIgnoreCase(dhattrid))
			deleteDHKeyStore(name, amiType);
	}

	public static void setKeyStore(AMI_KeyMgntService service, String name,
	    KeyStore ks, String keypass, String amiType) throws Exception {
		// %%% TBD
		setRSAKeyStore(service, name, ks, keypass, amiType);
	}

	/**
	 * Method to get AMI Object's profile
	 */
        protected static AMI_UserProfile getUserProfile(String name,
	    String amiType) throws Exception {
		// Debug information
		AMI_Debug.debugln(3, "Get profile for AMI Object: " + name);
		Attribute userProfileAttribute = getAttribute(name,
		    "objectprofile", amiType);
		if (userProfileAttribute == null)
			return (new AMI_UserProfile());
		Object obj = userProfileAttribute.get();
		byte[] userprofilebin = null;
		if (obj.getClass().getName().equals("java.lang.String"))
			userprofilebin = ((String) obj).getBytes();
		else
			userprofilebin = (byte[]) userProfileAttribute.get();
		return (new AMI_UserProfile(userprofilebin));
	}

	/**
	 * Method to set AMI Object's profile
	 */
        protected static void setUserProfile(AMI_KeyMgntService service,
	    String name, String amiType, AMI_UserProfile userProfile)
	    throws Exception {
		// Debug information
		AMI_Debug.debugln(3, "Set profile for AMI Object: " + name);
		byte[] userprofilebin = userProfile.toByteArray();
		deleteAndAddAttribute(service, name, "objectprofile",
		    userprofilebin, amiType);
	}

	/**
	 * Method to get default RSA signature key alias
	 */
        public static String getRSASignAlias(String name, String amiType)
	    throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		return (userProfile.getRSAsignkey());
        }

	/**
	 * Method to set default RSA signature key alias
	 */
        public static void setRSASignAlias(AMI_KeyMgntService service,
	    String name, String alias, String amiType) throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		userProfile.setRSAsignkey(alias);
		setUserProfile(service, name, amiType, userProfile);
        }

	/**
	 * Method to delete default RSA signature key alias
	 */
	public static void deleteRSASignAlias(AMI_KeyMgntService service,
	    String name, String amiType) throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		userProfile.removeRSAsignkey();
		setUserProfile(service, name, amiType, userProfile);
        }

	/**
	 * Method to get default RSA encryption key alias
	 */
        public static String getRSAEncryptAlias(String name, String amiType)
	    throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		return (userProfile.getRSAencryptkey());
        }

	/**
	 * Method to set default RSA encryption key alias
	 */
        public static void setRSAEncryptAlias(AMI_KeyMgntService service,
	    String name, String alias, String amiType) throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		userProfile.setRSAencryptkey(alias);
		setUserProfile(service, name, amiType, userProfile);
        }

	/**
	 * Method to delete default RSA signature key alias
	 */
        public static void deleteRSAEncryptAlias(AMI_KeyMgntService service,
	    String name, String amiType) throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		userProfile.removeRSAencryptkey();
		setUserProfile(service, name, amiType, userProfile);
        }

	/**
	 * Method to get default DSA key alias
	 */
        public static String getDSAAlias(String name, String amiType)
	    throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		return (userProfile.getDSAkey());
        }

	/**
	 * Method to set default DSA key alias
	 */
        public static void setDSAAlias(AMI_KeyMgntService service,
	    String name, String alias, String amiType) throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		userProfile.setDSAkey(alias);
		setUserProfile(service, name, amiType, userProfile);
        }

	/**
	 * Method to delete default DSA key alias
	 */
        public static void deleteDSAAlias(AMI_KeyMgntService service,
	    String name, String amiType) throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		userProfile.removeDSAkey();
		setUserProfile(service, name, amiType, userProfile);
        }

	/**
	 * Method to get default DH key alias
	 */
        public static String getDHAlias(String name, String amiType)
	    throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		return (userProfile.getDHkey());
        }

	/**
	 * Method to set default DH key alias
	 */
        public static void setDHAlias(AMI_KeyMgntService service,
	    String name, String alias, String amiType) throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		userProfile.setDHkey(alias);
		setUserProfile(service, name, amiType, userProfile);
        }

	/**
	 * Method to delete default DH key alias
	 */
        public static void deleteDHAlias(AMI_KeyMgntService service,
	    String name, String amiType) throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		userProfile.removeDHkey();
		setUserProfile(service, name, amiType, userProfile);
        }

	/**
	 * Method to set KeyStore File Name
	 */
        public static void setKeyStoreFileName(AMI_KeyMgntService service,
	    String name, String path, String amiType) throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		userProfile.setKeyStoreFile(path);
		setUserProfile(service, name, amiType, userProfile);
        }

	/**
	 * Method to delete KeyStore File Name
	 */
	public static void deleteKeyStoreFileName(AMI_KeyMgntService service,
	    String name, String amiType) throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		userProfile.removeKeyStoreFile();
		setUserProfile(service, name, amiType, userProfile);
        }

	/**
	 * Method to get KeyStore File Name
	 */
        public static String getKeyStoreFileName(String name, String amiType)
	    throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		return (userProfile.getKeyStoreFile());
        }

	/**
	 * Method to set Certificate file attribute
	 */
        public static void setCertificateFileName(AMI_KeyMgntService service,
	    String name, String path, String amiType) throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		userProfile.setCertificateFile(path);
		setUserProfile(service, name, amiType, userProfile);
        }

	/**
	 * Method to delete Certificate file attribute
	 */
	public static void deleteCertificateFileName(AMI_KeyMgntService service,
	    String name, String amiType) throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		userProfile.removeCertificateFile();
		setUserProfile(service, name, amiType, userProfile);
        }

	/**
	 * Method to get Certificate file attribute
	 */
        public static String getCertificateFileName(String name, String amiType)
	    throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		return (userProfile.getCertificateFile());
        }

	/**
	 * Method to set CA certificate file name
	 */
        public static void setCaCertificateFileName(AMI_KeyMgntService service,
	    String name, String path, String amiType) throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		userProfile.setCaCertificateFile(path);
		setUserProfile(service, name, amiType, userProfile);
        }

	/**
	 * Method to delete CA certificate file name
	 */
	public static void deleteCaCertificateFileName(
	    AMI_KeyMgntService service,
	    String name, String amiType) throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		userProfile.removeCaCertificateFile();
		setUserProfile(service, name, amiType, userProfile);
        }

	/**
	 * Method to get CA certificate file name
	 */
        public static String getCaCertificateFileName(String name,
	    String amiType) throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		return (userProfile.getCaCertificateFile());
        }

	/**
	 * Method to set Certificate Chain File Name
	 */
        public static void setCertificateChainFileName(
	    AMI_KeyMgntService service,
	    String name, String path, String amiType) throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		userProfile.setCertificateChainFile(path);
		setUserProfile(service, name, amiType, userProfile);
        }

	/**
	 * Method to delete Certificate Chain File Name
	 */
	public static void deleteCertificateChainFileName(
	    AMI_KeyMgntService service, String name, String amiType)
	    throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		userProfile.removeCertificateChainFile();
		setUserProfile(service, name, amiType, userProfile);
        }

	/**
	 * Method to get Certificate Chain File Name
	 */
        public static String getCertificateChainFileName(String name,
	    String amiType) throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		return (userProfile.getCertificateChainFile());
        }

	/**
	 * Method to set CertReq File Name
	 */
        public static void setCertReqFileName(AMI_KeyMgntService service,
	    String name, String path, String amiType) throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		userProfile.setCertReqFile(path);
		setUserProfile(service, name, amiType, userProfile);
        }

	/**
	 * Method to delete CertReq File Name
	 */
	public static void deleteCertReqFileName(AMI_KeyMgntService service,
	    String name, String amiType) throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		userProfile.removeCertReqFile();
		setUserProfile(service, name, amiType, userProfile);
        }

	/**
	 * Method to get CertReq File Name
	 */
        public static String getCertReqFileName(String name, String amiType)
	    throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		return (userProfile.getCertReqFile());
        }

	/**
	 * Method to set backup certificate file name
	 */
        public static void setBackupCertFileName(AMI_KeyMgntService service,
	    String name, String path, String amiType) throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		userProfile.setBackupCertificateFile(path);
		setUserProfile(service, name, amiType, userProfile);
        }

	/**
	 * Method to delete backup certificate file name
	 */
	public static void deleteBackupCertFileName(AMI_KeyMgntService service,
	    String name, String amiType) throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		userProfile.removeBackupCertificateFile();
		setUserProfile(service, name, amiType, userProfile);
        }

	/**
	 * Method to get backup certificate file name
	 */
        public static String getBackupCertFileName(String name, String amiType)
	    throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		return (userProfile.getBackupCertificateFile());
        }

	/**
	 * Method to set loginName to DN name aliases
	 * and HostIP to DN name aliases
	 */
	public static void setNameDNAlias(AMI_KeyMgntService service,
	    String name, String alias, String amiType) throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		userProfile.setNameDNAlias(alias);
		setUserProfile(service, name, amiType, userProfile);
        }

	/**
	 * Method to delete loginName to nameDN aliases
	 */
	public static void deleteNameDNAlias(AMI_KeyMgntService service,
	    String name, String alias, String amiType) throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		userProfile.removeNameDNAlias(alias);
		setUserProfile(service, name, amiType, userProfile);
        }

	/**
	 * Method to get nameDN alias
	 */
        public static String[] getNameDNAlias(String name, String alias,
	    String amiType) throws Exception {
		AMI_UserProfile userProfile = getUserProfile(name, amiType);
		return (userProfile.getNameDNAlias(alias));
        }

	/**
	 * Method to get an enumeration of X509 Certificates
	 */
	public static Enumeration getX509Certificates(String name,
	    String amiType) throws Exception {
		// Get the attributes for x509 certificates
		Attribute certs = getAttribute(name, "certX509", amiType);
		if (certs == null)
			return (null);

		NamingEnumeration enum = certs.getAll();
		if (enum == null)
			return (null);

		Vector certificates = new Vector();
		byte[] cert_encoded;
		Object obj;
		X509Certificate certificate;
		while (enum.hasMore()) {
			obj = enum.next();
			if (obj.getClass().getName().equals(
			    "java.lang.String"))
				cert_encoded = ((String) obj).getBytes();
			else
				cert_encoded = (byte[]) obj;
			ByteArrayInputStream bais =
			    new ByteArrayInputStream(cert_encoded);
			CertificateFactory cf =
			    CertificateFactory.getInstance("X509", "SunAMI");
			certificate = (X509Certificate)
			    cf.generateCertificate(bais);
			certificates.addElement(certificate);
		}
		return (certificates.elements());
	}

	/**
	 * Method to add X509 certificate to the directory
	 */
	public static void addX509Certificate(AMI_KeyMgntService service,
	    String name, X509Certificate cert, String amiType)
	    throws Exception {
		addAttribute(service, name, "certX509",
		    cert.getEncoded(), amiType);
	}

	/**
	 * Method to delete X509 certificate to the directory
	 */
	public static void deleteX509Certificate(String name,
	    X509Certificate cert, String amiType) throws Exception {
		if (cert != null)
			deleteAttribute(name, "certX509",
			    cert.getEncoded(), amiType);
		else
			deleteAttribute(name, "certX509", null, amiType);
	}

	/**
	 * Method to add certficate request to the directory service
	 */
	public static void addCertificateRequest(AMI_KeyMgntService service,
	    String name, AMI_CertReq certreq, String amiType)
	    throws Exception {
		addAttribute(service, name, "certreq",
		    certreq.toByteArray(), amiType);
	}

	/**
	 * Method to delete certificate requests from the directory
	 */
	public static void deleteCertificateRequest(String name,
	    AMI_CertReq certreq, String amiType) throws Exception {
		if (certreq != null)
			deleteAttribute(name, "certreq",
			    certreq.toByteArray(), amiType);
		else
			deleteAttribute(name, "certreq", null, amiType);
	}

	/**
	 * Method to get Certificate requests from the directory
	 */
	public static Enumeration getCertificateRequests(String name,
	    String amiType) throws Exception {
		Attribute certReqsAttr = getAttribute(name, "certreq", amiType);
		if (certReqsAttr == null)
			return (null);

		// Construct a vector and return the enumeration
		Vector certreqs = new Vector();
		Object obj;
		byte[] certreq_encoded;
		AMI_CertReq certreq;
		NamingEnumeration enum = certReqsAttr.getAll();
		while (enum.hasMore()) {
			obj = enum.next();
			if (obj.getClass().getName().equals(
			    "java.lang.String"))
				certreq_encoded = ((String) obj).getBytes();
			else
				certreq_encoded = (byte[]) obj;
			certreq = new AMI_CertReq(certreq_encoded);
			certreqs.addElement(certreq);
		}
		return (certreqs.elements());
	}

	/**
	 * Method to backup Certificate Requests
	 */
	public static void backupCertificateRequest(AMI_KeyMgntService service,
	    String name, AMI_CertReq certreq, String amiType)
	    throws Exception {
		addAttribute(service, name, "backupCertReqCerts",
		    certreq.toByteArray(), amiType);
	}

	/**
	 * Method to backup Certificates
	 */
	public static void backupCertificate(AMI_KeyMgntService service,
	    String name, java.security.cert.Certificate cert, String amiType)
	    throws Exception {
		addAttribute(service, name, "backupCertReqCerts",
		    cert.getEncoded(), amiType);
	}

	/**
	 * Method to create AMI Object
	 */
	protected static void createAmiObject(String nameLogin,
	    String nameDN, String nameDNS, String amiType, AMI_KeyMgnt keymgnt)
	    throws Exception {
		// Attribute set that be added to the directory
		Attributes basicAttributes = new BasicAttributes();

		// Add the name attributes to the attributeset
		Attribute attribute;
		if (nameLogin != null) {
			if (amiType.equalsIgnoreCase(
			    AMI_Constants.AMI_USER_OBJECT)) {
				attribute = new BasicAttribute("nameuser",
				    nameLogin.toLowerCase());
			} else if (amiType.equalsIgnoreCase(
			    AMI_Constants.AMI_HOST_OBJECT)) {
				attribute = new BasicAttribute("namehost",
				    nameLogin.toLowerCase());
			} else
				attribute = new BasicAttribute(
				    "nameapplication",
				    nameLogin.toLowerCase());
			basicAttributes.put(attribute);
		}
		if (nameDN != null) {
			attribute = new BasicAttribute("namedn",
			    normalizeDN(nameDN));
			basicAttributes.put(attribute);
		}
		if (nameDNS != null) {
			attribute = new BasicAttribute("namedns",
			    nameDNS.toLowerCase());
			basicAttributes.put(attribute);
		}
		keymgnt.create(nameLogin.toLowerCase(), basicAttributes);
	}

	/**
	 * Method to delete AMI Object
	 */
	public static void deleteAmiObject(String name, String amiType)
	    throws Exception {
		try {
			deleteAttribute(name, "keystorersa", null, amiType);
		} catch (Exception e) { }
		try {
			deleteAttribute(name, "keystoredsa", null, amiType);
		} catch (Exception e) { }
		try {
			deleteAttribute(name, "keystoredh", null, amiType);
		} catch (Exception e) { }
		try {
			deleteAttribute(name, "objectprofile", null, amiType);
		} catch (Exception e) { }
		try {
			deleteAttribute(name, "certx509", null, amiType);
		} catch (Exception e) { }
		try {
			deleteAttribute(name, "certreqrsa", null, amiType);
		} catch (Exception e) { }
		try {
			deleteAttribute(name, "certreqdsa", null, amiType);
		} catch (Exception e) { }
		try {
			deleteAttribute(name, "certreqdh", null, amiType);
		} catch (Exception e) { }

		// ami.destroy(indexName);
	}
	
	public static String normalizeDN(String nameDN) {
		String answer = new String("");
		if (nameDN == null)
			return (null);

		// If NOT a distinguished name, return lower case.
		if (nameDN.indexOf('=') == -1)
			return (nameDN.toLowerCase());

		if (nameDN.charAt(0) == '\"') {
			nameDN = new String(nameDN.substring(1));
		}
		if (nameDN.charAt(nameDN.length()-1) == '\"') {
			nameDN = new String(nameDN.substring(0,
			    nameDN.length()-1));
		}

		String uppercaseDN = nameDN.toUpperCase();
		StringTokenizer tokens = new StringTokenizer(
		    uppercaseDN, ",", true);
		String rdn, items;
		StringTokenizer rdnTokens;
		while (tokens.hasMoreTokens()) {
			rdn = tokens.nextToken().trim();
			rdnTokens = new StringTokenizer(rdn, "=", true);
			while (rdnTokens.hasMoreTokens())
				answer += rdnTokens.nextToken().trim();
		}
		return (answer);
	}

	protected static String getDNNameFromAlias(String loginName)
	    throws Exception {
		String name = System.getProperty("user.name");
		String type = AMI_Constants.AMI_USER_OBJECT;
		if (AMI_KeyMgnt_FNS.fns_get_euid(null) == 0) {
			name = AMI_VirtualHost.getHostIP();
			type = AMI_Constants.AMI_HOST_OBJECT;
		}
		String[] names = getNameDNAlias(name, loginName, type);
		if (names != null) {
			int index = names[0].indexOf('=');
			return (names[0].substring(index + 1));
		}
		return (null);
	}

	/**
	 * Method to current user's DN name
	 */
	public static String getDNName() throws Exception {
		String amiType = AMI_Constants.AMI_USER_OBJECT;
		String name = System.getProperty("user.name");
		int uid = AMI_KeyMgnt_FNS.fns_get_euid(null);
		if (uid == 0) {
			amiType = AMI_Constants.AMI_HOST_OBJECT;
			name = AMI_VirtualHost.getHostIP();
		}
		return (getNameFromLoginName(name, amiType,
		    "namedn"));
	}

	/**
	 * Method to Distinguished Name from Login Name
	 */
	public static String getDNNameFromLoginName(String loginName)
	    throws Exception {
		String answer;

		// First check locally in profile alias
		if ((answer = getDNNameFromAlias(loginName)) != null)
			return (answer);

		/* Obtain the uid of the user using a JNI call */
		String amiType = AMI_Constants.AMI_USER_OBJECT;
		if (AMI_KeyMgnt_FNS.fns_get_euid(loginName) == 0)
			amiType = AMI_Constants.AMI_HOST_OBJECT;
		return (getNameFromLoginName(loginName, amiType,
		    "namedn"));
	}

	/**
	 * Method to Distinguished Name from Login Name
	 */
	public static String getDNNameFromLoginName(String loginName,
	    String amiType) throws Exception {
		// First check locally in profile alias
		String answer;
		if ((answer = getDNNameFromAlias(loginName)) != null)
			return (answer);

		// Try searching the directory service
		answer = getNameFromLoginName(loginName, amiType, "namedn");
		return (answer);
	}

	/**
	 * Method to DNS Name from Login Name
	 */
	public static String getDNSNameFromLoginName(String loginName,
	    String amiType) throws Exception {
		return (getNameFromLoginName(loginName, amiType, "namedns"));
	}

	protected static String getNameFromLoginName(String loginName,
	    String amiType, String reqName) throws Exception {
		String indexName = null;
		AMI_KeyMgnt amiKeyMgnt = null;
		Attribute attr = null;

		if ((indexName = getIndexName(loginName, "certx509", amiType))
		    != null) {
			amiKeyMgnt = getAMIKeyMgntInstance("certx509", amiType);
			if ((attr = amiKeyMgnt.getAttribute(indexName, reqName))
			    != null)
				return ((String) attr.get());
		}

		if ((indexName = getIndexName(loginName, "keystorersa",
		    amiType)) != null) {
			amiKeyMgnt = getAMIKeyMgntInstance("keystorersa",
			    amiType);
			if ((attr = amiKeyMgnt.getAttribute(indexName, reqName))
			    != null)
				return ((String) attr.get());
		}

		if ((indexName = getIndexName(loginName, "keystoredsa",
		    amiType)) != null) {
			amiKeyMgnt = getAMIKeyMgntInstance("keystoredsa",
			    amiType);
			if ((attr = amiKeyMgnt.getAttribute(indexName, reqName))
			    != null)
				return ((String) attr.get());
		}
	
		if ((indexName = getIndexName(loginName, "keystoredh", amiType))
		    != null) {
			amiKeyMgnt = getAMIKeyMgntInstance("keystoredh",
			    amiType);
			if ((attr = amiKeyMgnt.getAttribute(indexName, reqName))
			    != null)
				return ((String) attr.get());
		}
		return (null);
	}

	/**
	 * Method to get the amiType form name
	 */
	public static String getAmiType(String name, String attrType)
	    throws Exception {
		// Try all the possible combinations
		String nameType = new String("nameuser");
		if (name.indexOf('=') != -1) {
			// It is distinguished name
			nameType = new String("namedn");
			name = normalizeDN(name);
		// } else if ((name.indexOf('.') != -1) ||
		//    (name.indexOf('@') != -1)) {
			// It is a DNS name
		//	nameType = new String("namedns");
		}

		// Try USER object
		if (searchForAmiType(name, nameType, attrType,
		    AMI_Constants.AMI_USER_OBJECT) != null)
			return (AMI_Constants.AMI_USER_OBJECT);

		// Try HOST object
		if (nameType.equals("nameuser"))
			nameType = new String("namehost");
		if (searchForAmiType(name, nameType, attrType,
		    AMI_Constants.AMI_HOST_OBJECT) != null)
			return (AMI_Constants.AMI_HOST_OBJECT);

		// Try APPLICATION object
		if (nameType.equals("namehost"))
			nameType = new String("nameapplication");

		if (searchForAmiType(name, nameType, attrType,
		    AMI_Constants.AMI_APPLICATION_OBJECT) != null)
		        return (AMI_Constants.AMI_APPLICATION_OBJECT);
		else
		  	return (AMI_Constants.AMI_USER_OBJECT);

	}

	protected static String searchForAmiType(String name, String nameType,
	    String attrType, String amiType) throws Exception {
		// Construct the search attribute
		Attribute  searchAttribute = new BasicAttribute(
		    nameType, name);

		// Get AMI KeyMgnt for amiType
		AMI_KeyMgnt ami = getAMIKeyMgntInstance(attrType, amiType);

                // Perform the AMI search operation
		Enumeration enum = null;
		try {
			enum = ami.search(searchAttribute);
		} catch (AMI_KeyMgntCommunicationException ne) {
			amiKeyMgntObjects.clear();
			ami = getAMIKeyMgntInstance(attrType,
			    AMI_Constants.AMI_USER_OBJECT);
			enum = ami.search(searchAttribute);
		}

		if ((enum != null) && (enum.hasMoreElements())) {
                        // Object found
                        return (amiType);
                }
		return (null);
	}

	/**
	 * Method to get a dummy implementation of AMI_KeyMgntService
	 */
	public static AMI_KeyMgntService getKeyMgntServiceDummyImpl() {
		return (new AMI_KeyMgntServiceImpl());
	}
}
