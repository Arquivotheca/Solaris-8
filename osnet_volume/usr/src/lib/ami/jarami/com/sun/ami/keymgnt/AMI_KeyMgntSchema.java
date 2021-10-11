/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_KeyMgntSchema.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.keymgnt;

import java.util.Enumeration;
import java.util.Vector;
import java.util.Properties;
import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

public class AMI_KeyMgntSchema extends Object {

	// Properties defined for Schema
	public static final String AMI_KEYMGNT_SCHEMA_PREFIX =
	    "ami.keymgnt.schema.";
	public static final String AMI_KEYMGNT_SCHEMA_FILENAME =
	    ".filename";

	// Method Name
	public static final String AMI_KEYMGNT_SCHEMA_METHOD_NAME =
	    "AMI_KeyMgntSchema_";

	protected String schemaName;
	protected Vector attributeSchema;
	protected Properties environment;

	public AMI_KeyMgntSchema(String schema, String protocol,
	    Properties env) throws AMI_KeyMgntSchemaException {
		schemaName = new String(schema);
		environment = new Properties();
		if (env != null) {
			/* environment.putAll(env); (only in JDK1.2) */
			Enumeration allKeys = env.keys();
			while (allKeys.hasMoreElements()) {
				String nextKey = (String) allKeys.nextElement();
				environment.put(nextKey,
				    env.getProperty(nextKey));
			}
		}

		// Construct the protocol method name and invoke
		String methodName = new String(AMI_KEYMGNT_SCHEMA_METHOD_NAME +
		    protocol.toUpperCase());
		try {
			Method method = (this.getClass()).getMethod(methodName,
			    null);
			method.invoke(this, null);
		} catch (NoSuchMethodException e) {
			System.out.println("Method: " + methodName);
			throw new AMI_KeyMgntSchemaException(
			    "No such protocol " + e.toString());
		} catch (IllegalAccessException f) {
			throw new AMI_KeyMgntSchemaException(
			    "Illegal access to protocol " + f.toString());
		} catch (InvocationTargetException g) {
			throw new AMI_KeyMgntSchemaException(
			    "Unable to invoke the protocol " + g.toString() +
			    "\n" + g.getTargetException().toString());
		}
	}

	public AMI_KeyMgntSchema(AMI_KeyMgntSchema schema) {
		schemaName = new String(schema.schemaName);

		/* attributeSchema = new Vector(schema.attributeSchema); */
		attributeSchema = new Vector();
		Enumeration allSchema = schema.attributeSchema.elements();
		while (allSchema.hasMoreElements())
			attributeSchema.addElement(allSchema.nextElement());

		environment = new Properties();
		/* environment.putAll(schema.environment); */
		Enumeration allKeys = schema.environment.keys();
		while (allKeys.hasMoreElements()) {
			String key = (String) allKeys.nextElement();
			environment.put(key,
			    schema.environment.getProperty(key));
		}
	}

	public String getSchemaName() {
		return (new String(schemaName));
	}

	public AMI_KeyMgntSchemaEntry getAttributeSchema(String attrID)
	    throws AMI_KeyMgntSchemaNotFoundException {
		if (attributeSchema == null)
			throw new AMI_KeyMgntSchemaNotFoundException(
			    "Schema not found for: " + attrID);
		AMI_KeyMgntSchemaEntry entry =
		    new AMI_KeyMgntSchemaEntry(attrID);
		int index = attributeSchema.indexOf(entry);
		if (index == -1)
			return (null);
		return ((AMI_KeyMgntSchemaEntry)
		    attributeSchema.elementAt(index));
	}

	public Enumeration getAllSchemaEntries()
	    throws AMI_KeyMgntSchemaException {
		if (attributeSchema == null)
			throw new AMI_KeyMgntSchemaException(
			    "No Schema Elements");
		return (attributeSchema.elements());
	}

	public int getNumSchemaEntries() {
		return (attributeSchema.size());
	}

	public String toString() {
		String answer = new String("Schema Name: " + schemaName);
		Enumeration enum = null;
		try {
			enum = getAllSchemaEntries();
		} catch (AMI_KeyMgntSchemaException e) {
			answer += "\nUnable to enumerate schema entries: "
			    + e.toString();
		}
		while (enum.hasMoreElements()) {
			AMI_KeyMgntSchemaEntry entry =
			    (AMI_KeyMgntSchemaEntry) enum.nextElement();
			answer += "\n\n" + entry.getAttributeID() + ": ";
			if (entry.isBinary())
				answer += "(binary,";
			else
				answer += "(ascii,";
			if (entry.isSearchable())
				answer += "searchable,";
			if (entry.isMultiValued())
				answer += "multivalued,";
			else
				answer += "single-valued,";
			if (entry.isOwnerChangable())
				answer += "owner-changable,";
			if (entry.isAdminChangeable())
				answer += "admin-changable,";
			if (entry.isEncrypted())
				answer += "encrypted)";
			else
				answer += "non-encrypted)";
		}
		return (answer);
	}


	// Protocols
	public void AMI_KeyMgntSchema_FILES()
	    throws AMI_KeyMgntSchemaException {
		String FileName = null;
		String propIndex = new String(AMI_KEYMGNT_SCHEMA_PREFIX +
		    schemaName.toLowerCase() + AMI_KEYMGNT_SCHEMA_FILENAME);
		if ((environment == null) || ((FileName =
		    environment.getProperty(propIndex)) == null)) {
			throw new AMI_KeyMgntSchemaException("Property " +
			    "ami.keymgnt.schema.<schema>.filename" +
			    " must be defined with the schema filename");
		}
		attributeSchema = new Vector(10);
		Enumeration enum =
		    new AMI_KeyMgntSchema_Files_Enumeration(FileName);
		while (enum.hasMoreElements())
			attributeSchema.addElement(enum.nextElement());
	}
}
