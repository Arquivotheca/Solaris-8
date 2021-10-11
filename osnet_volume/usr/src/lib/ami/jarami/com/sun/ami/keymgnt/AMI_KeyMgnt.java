/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_KeyMgnt.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.keymgnt;

/**
 * This interface represents Key Management operations which
 * consists of add/modify/delete operations to private keys
 * (in the form of keystore) and public keys (in the form of
 * certificates.
 */

import java.lang.*;
import java.lang.reflect.*;
import java.util.*;
import java.io.*;
import javax.naming.*;
import javax.naming.directory.*;

import com.sun.ami.common.AMI_Common;

/* 
	AMI_KeyMgnt Properties
ami.keymgnt.properties	-- File that contains additions properties
ami.keymgnt.datastore	-- Backend data store ie., ldap, dns, files

	Algorithm to obtain the attribute identifier
ami.keymgnt.datastore.attr	-- Attribute identifier

eg.,
ami.keymgnt.files.keystore = onc_keypackages
ami.keymgnt.files.certificate = onc_certificate
...

*/

public abstract class AMI_KeyMgnt extends AMI_Common {

	public final static String AMI_OBJECT_PATH =
	    "ami.keymgnt.instance.prefix";
	public final static String AMI_PROPERTIES_FILE =
	    "ami.keymgnt.properties";
	public final static String AMI_DATASTORE = "ami.keymgnt.datastore";
	public final static String AMI_DATASTORE2 = "ami.keymgnt.datastore2";
	public final static String AMI_PROPERTY_PREFIX = "ami.keymgnt.";
	public final static String AMI_OBJECTCLASS = "objectclass";
	public final static String AMI_OBJECTCLASS_VALUE = "objectclass.value";
	public final static String AMI_ADDITIONAL_OC = "addoc";
	public final static String AMI_ADDITIONAL_ATTR = "addattr";
	public final static String AMI_INDEXNAME = "indexname";

	Properties environment = null;
	Properties schemaProperties = null;
	AMI_KeyMgntSchema schema = null;
	// String amiDatastore = null;
	String amiPropertyIndex = null;

	/**
	  * getInstance() method
	  */

	public static AMI_KeyMgnt getInstance(String iname,
	    AMI_KeyMgntSchema schema, Properties ienv)
	    throws AMI_KeyMgntException {

		// Mapping rules, for NIS, NIS+, files use FNS.
		// For LDAP (in future DNS, COS naming) using JNDI
		String name = null;
		if (iname.equalsIgnoreCase("ldap"))
			name = new String("JNDI");
		else if (iname.equalsIgnoreCase("fns") ||
		    iname.equalsIgnoreCase("nis") ||
		    iname.equalsIgnoreCase("nisplus")) {
			name = new String("FNS");
		} else if (iname.equalsIgnoreCase("file"))
			name = new String("FILE");
		else {
			throw new AMI_KeyMgntException(
			    "Unknow name service: " + iname);
		}

		// Set datastore properties
		String dataStore = iname.toLowerCase();
		String dataStore2 = null;
		if (name.equals("FNS")) {
			if (dataStore.equals("nis"))
				dataStore2 = new String("nis");
			else if (dataStore.equals("nisplus"))
				dataStore2 = new String("nisplus");
			else
				dataStore2 = new String("files");
			dataStore = new String("fns");
		}
		Properties env = new Properties();
 		if (ienv != null)
			env.putAll(ienv);
		env.put(AMI_DATASTORE, dataStore);
		if (dataStore2 != null)
			env.put(AMI_DATASTORE2, dataStore2);

		// Construct the className, load the constructor and getInstance
		AMI_KeyMgnt keyMgnt = null;
		String className = null;
		try {
			Class cl, params[];
			Object inputs[];
			Constructor constructor;

			className = new String(env.getProperty(AMI_OBJECT_PATH,
			    "com.sun.ami.keymgnt.AMI_KeyMgnt_") + name);
			cl = Class.forName(className);

			params = new Class[2];
			params[0] = schema.getClass();
			params[1] = env.getClass();

			constructor = cl.getConstructor(params);

			inputs = new Object[2];
			inputs[0] = schema;
			inputs[1] = env;

			keyMgnt = (AMI_KeyMgnt) constructor.newInstance(inputs);
		} catch (ClassNotFoundException e) {
			throw new AMI_KeyMgntNotFoundException(
			    "class " + className + " for AMI_KeyMgnt " +
			    "not found\n" + e.getMessage());
		} catch (InstantiationException f) {
			throw new AMI_KeyMgntNotFoundException(
			    "class " + className + " cannot be instantiated\n" +
			    f.getMessage());
		} catch (IllegalAccessException g) {
			throw new AMI_KeyMgntNotFoundException(
			    "class " + className + " cannot be accessed\n" +
			    g.getMessage());
		} catch (SecurityException h) {
			throw new AMI_KeyMgntNotFoundException(
			    "class " + className + " has security problems\n" +
			    h.getMessage());
		} catch (InvocationTargetException i) {
			// i.printStackTrace();
			throw new AMI_KeyMgntNotFoundException(
			    "class " + className + " cannot be invoked\n" +
			    i.getMessage() + "\n" +
			    i.getTargetException().toString());
		} catch (NoSuchMethodException j) {
			throw new AMI_KeyMgntNotFoundException(
			    "class " + className +
			    " does not have constructor\n" +
			    j.getMessage());
		}
		return (keyMgnt);
	}

	public AMI_KeyMgnt() {
	}

	protected AMI_KeyMgnt(AMI_KeyMgntSchema ischema, Properties props)
	    throws AMI_KeyMgntException {

		// Set the amiPropertyIndex ie., backend + schema
		// ie., fns.ami.
		amiPropertyIndex = new String(
		    props.getProperty(AMI_KeyMgnt.AMI_DATASTORE) + "." +
		    (ischema.getSchemaName()).toLowerCase() + ".");

		// Copy the Schema
		schema = new AMI_KeyMgntSchema(ischema);

		// Obtain the properties
		environment = new Properties();
		environment.putAll(props);

		// Check if we need to read additional properties from a file
		String filename = null;
		if ((filename = props.getProperty(AMI_PROPERTIES_FILE))
		    != null) {
			// Read from this file
			File pfile = new File(filename);
			if (pfile.exists() && pfile.canRead()) {
				BufferedInputStream propfile = null;
				try {
				    propfile = new BufferedInputStream(
				    new FileInputStream(pfile));
				} catch (FileNotFoundException fnfe) {
					throw new AMI_KeyMgntException(
					    "File not found: " +
					    fnfe.getMessage());
				}
				Properties additionalProp = new Properties();
				try {
					additionalProp.load(propfile);
					environment.putAll(additionalProp);
					propfile.close();
				} catch (IOException ioe) {
					throw new AMI_KeyMgntException(
					    "Unable to load property: " +
					    ioe.getMessage());
				}
			}
		}

		// Copy the reverse schema mapping
		String schemaID = null, schemaProperty, schemaValue,
		    attrIDPrefix;
		schemaProperties = new Properties();
		attrIDPrefix = new String(AMI_PROPERTY_PREFIX +
		    amiPropertyIndex);
		// First copy the objectClass identifier
		schemaProperty = new String(attrIDPrefix + AMI_OBJECTCLASS);
		if ((schemaValue = environment.getProperty(schemaProperty))
		    != null)
			schemaProperties.put(schemaValue, AMI_OBJECTCLASS);
		// Construct the reamining attribtues defined in schema
		Enumeration schemaEnum = schema.getAllSchemaEntries();
		while (schemaEnum.hasMoreElements()) {
			schemaID = ((AMI_KeyMgntSchemaEntry)
			    schemaEnum.nextElement()).getAttributeID();
			schemaProperty = new String(attrIDPrefix + schemaID);
			schemaValue = environment.getProperty(schemaProperty);
			if (schemaValue != null)
				schemaProperties.put(schemaValue, schemaID);
		}
	}

	/**
	  * Method to create a sub-context
	  */
	public abstract void create(String name, Attributes attrset)
	    throws AMI_KeyMgntException;

	/**
	  * Method to destroy a sub-context
	  */
	public abstract void destroy(String name) throws AMI_KeyMgntException;

	/**
	  * Method to add attributes
	  */
	public abstract void addAttributes(String name, Attributes attrset)
	    throws AMI_KeyMgntException;

	/**
	  * Method to delete attributes
	  */
	public abstract void deleteAttributes(String name, Attributes attrset)
	    throws AMI_KeyMgntException;

	/**
	  * Method to get all attributes
	  */
	public abstract Attributes getAttributes(String name)
	    throws AMI_KeyMgntException;

	/**
	  * Method to get specific attribute
	  */
	public abstract Attribute getAttribute(String name, String attrID)
	    throws AMI_KeyMgntException;

	/**
	  * Method to list all sub-contexts
	  */
	public abstract Enumeration list() throws AMI_KeyMgntException;

	/**
	  * Method to search for sub-contexts having the specified attribute
	  */
	public abstract Enumeration search(Attribute attribute)
	    throws AMI_KeyMgntException;


	// Routine to get the indexName ie., name of the context from the
	// the list of attribtues, to create the sub-context
	protected String getIndexName(String inName, Attributes attrset)
	    throws AMI_KeyMgntException, NamingException {
		if (inName != null)
			return (new String(inName));

		String indexName = new String(AMI_PROPERTY_PREFIX +
		    amiPropertyIndex + AMI_INDEXNAME);
		String id = environment.getProperty(indexName);
		if (id == null)
			throw new AMI_KeyMgntException(
			    "Property that defines the context " +
			    "name attribute not defined in the " +
			    "properties file");

		Attribute attr = attrset.get(id.toLowerCase());
		if (attr == null)
			throw new AMI_KeyMgntException(
			    "Name to create context not provided: " +
			    "No attribtue -- " + id);
		NamingEnumeration enum = attr.getAll();
		if (enum == null)
			throw new AMI_KeyMgntException(
			    "Name to create context not provided: No values");
		String name = (String) enum.nextElement();
		return (name);
	}

	// Routines to mangle the attribute IDs to suit the name service
	protected Attribute getAttributeForNamingService(Attribute attr)
	    throws AMI_KeyMgntException, NamingException {
		Attribute newAttr;
		String attrID, attrIndex, attrNewID;

		attrID = attr.getID();

		// %%% Donot do the schema checking
		// if (schema.getAttributeSchema(attrID) == null)
		// throw new AMI_KeyMgntException("Attribute ID: " +
		// attrID + " is not defined in the schema");

		attrIndex = new String(AMI_PROPERTY_PREFIX + amiPropertyIndex +
		     attr.getID());
		if ((attrNewID = environment.getProperty(attrIndex))
		    == null)
			return (attr);
		else {
			newAttr = new BasicAttribute(attrNewID);
			for (NamingEnumeration attrEnum = attr.getAll();
			    attrEnum.hasMore(); newAttr.add(attrEnum.next()));
			return (newAttr);
		}
	}

	protected Attributes getAttributesForNamingService(Attributes attrs)
	    throws AMI_KeyMgntException, NamingException {
		Attributes answer = new BasicAttributes();
		Attribute attribute = null;

		NamingEnumeration attrsEnum = attrs.getAll();
		while (attrsEnum.hasMore()) {
			attribute = (Attribute) attrsEnum.next();
			answer.put(getAttributeForNamingService(attribute));
		}
		return (answer);
	}

	// Routine to demange the attribute IDs to suit the schema definitions
	protected Attribute getAttributeForApplication(Attribute attr)
	    throws NamingException {
		Attribute newAttr;
		String attrNewID;

		if ((attrNewID =
		    schemaProperties.getProperty(attr.getID())) == null)
			return (attr);
		else {
			newAttr = new BasicAttribute(attrNewID);
			for (NamingEnumeration attrEnum = attr.getAll();
			    attrEnum.hasMore(); newAttr.add(attrEnum.next()));
			return (newAttr);
		}
	}

	protected Attributes getAttributesForApplication(Attributes attrs)
	    throws NamingException {
		Attributes answer = new BasicAttributes();
		Attribute attr = null;

		NamingEnumeration attrsEnum = attrs.getAll();
		while (attrsEnum.hasMore()) {
			attr = (Attribute) attrsEnum.next();
			answer.put(getAttributeForApplication(attr));
		}
		return (answer);
	}

	protected String getAttributeIDForNamingService(String schemaNameID) {
		String attrIndex = new String(AMI_PROPERTY_PREFIX +
		    amiPropertyIndex + schemaNameID);
		return (environment.getProperty(attrIndex, schemaNameID));
	}
}
