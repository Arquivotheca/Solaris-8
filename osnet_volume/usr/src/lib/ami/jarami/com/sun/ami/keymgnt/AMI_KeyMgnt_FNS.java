/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_KeyMgnt_FNS.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.keymgnt;

import java.util.Enumeration;
import java.util.Properties;
import java.util.Vector;
import javax.naming.*;
import javax.naming.directory.*;

public class AMI_KeyMgnt_FNS extends AMI_KeyMgnt {

	/*
	 * Native methods defined for FNS.
	 * returns 1 on SUCCESS and 0 on FAILURE.
	 * Currently they donot throw an exception
	 */
	native int fns_set_naming_context(String ns, String ctx);
	native int fns_context_handle_destroy();
	native int fns_add_attribute(String name, String attrID,
	    String attrValue, int searchable);
	native int fns_add_binary_attribute(String name,
	    String attrID, byte[] attrValue);
	native int fns_delete_attribute(String name,
	    String attrID, String attrValue);
	native int fns_delete_binary_attribute(String name,
	    String attrID, byte[] attrValue);
	native int fns_destroy_context(String name);
	native String[] fns_get_attributeIDs(String name);
	native Object[] fns_get_attribute(String name, String attrID);
	native String[] fns_search(String attrID, String attrValue);

	/* Native method to obtain the UID of the user */
	public native static int fns_get_euid(String name);

	protected final static String FNS_CTX_PREFIX =
	    "ami.keymgnt.fns.ami.directoryRoot";

	/* FNS context */
	protected byte[] context;

	// Methods for RMI implementation
	public AMI_KeyMgnt_FNS() {
	}

	public void set_naming_context(String ns, String ctx)
	    throws AMI_KeyMgntException {
		if (fns_set_naming_context(ns, ctx) == 0)
			throw new AMI_KeyMgntException(
			    "Unable to get initial context");
	}

	public AMI_KeyMgnt_FNS(AMI_KeyMgntSchema ischema, Properties env)
	    throws AMI_KeyMgntException {
		super(ischema, env);

		// Check if any naming service properties are specified
		String dataStore = environment.getProperty(AMI_DATASTORE2);
		String ctxPrefix = environment.getProperty(FNS_CTX_PREFIX);
		if ((dataStore == null) || (ctxPrefix == null)) {
			throw new AMI_KeyMgntException(
			    "Unable to get the name service or " +
			    "prefix property");
		}
		// Set the name service for FNS
		fns_set_naming_context(dataStore, ctxPrefix);
	}

	public void create(String name, Attributes attrset)
	    throws AMI_KeyMgntException {
		addAttributes(name, attrset);
	}

	public void destroy(String name) throws AMI_KeyMgntException {
		if (fns_destroy_context(name) != 1)
			throw new AMI_KeyMgntException(
			    "Error in FNS destroy context " + name);
	}

	protected void addAttribute(String name, Attribute attribute)
	    throws AMI_KeyMgntException {
		String attrID = attribute.getID();
		try {
			NamingEnumeration enum = attribute.getAll();
			while (enum.hasMore()) {
				Object obj = enum.next();
				if (obj.getClass().getName().equals(
				    "java.lang.String")) {
					if (fns_add_attribute(name, attrID,
					    ((String) obj), 1) != 1) {
						throw new AMI_KeyMgntException(
						    "Error in FNS add " +
						    "attribute: "
						    + attrID + " " + obj);
					}
				} else {
					if (fns_add_binary_attribute(name,
					    attrID, ((byte[]) obj)) != 1) {
						throw new AMI_KeyMgntException(
						    "Error in FNS adding " +
						    "binary attribute: " +
						    attrID);
					}
				}
			}
		} catch (NamingException e) {
			throw new AMI_KeyMgntException(e.toString());
		}
	}

	protected void checkAndAddObjectClassAttribute(String name)
	    throws AMI_KeyMgntException {
		String attrIdIndex = new String(AMI_PROPERTY_PREFIX +
		    amiPropertyIndex + AMI_OBJECTCLASS);
		String attrID = environment.getProperty(attrIdIndex,
		    AMI_OBJECTCLASS);

		if (getAttribute(name, attrID) != null)
			return;

		// Add the object class attribute
		String attrValueIndex = new String(AMI_PROPERTY_PREFIX +
		    amiPropertyIndex + AMI_OBJECTCLASS_VALUE);
		String value = new String("AMI_KeyMgnt_" +
		    amiPropertyIndex.toUpperCase());
		String attrValue = environment.getProperty(attrValueIndex,
		    value);
		Attribute attribute = new BasicAttribute(attrID, attrValue);
		addAttribute(name, attribute);
	}

	public void addAttributes(String iname, Attributes attrset)
	    throws AMI_KeyMgntException {
		Attribute attribute;
		try {
			String name = getIndexName(iname, attrset);
			checkAndAddObjectClassAttribute(name);
			NamingEnumeration enum = attrset.getAll();
			while (enum.hasMore()) {
				attribute = (Attribute) enum.next();
				addAttribute(name,
				    getAttributeForNamingService(attribute));
			}
		} catch (NamingException e) {
			throw new AMI_KeyMgntException(e.toString());
		}
	}

	protected void deleteAttribute(String name, Attribute attribute)
	    throws AMI_KeyMgntException {
		String attrID = attribute.getID();
		try {
			int count = attribute.size();
			NamingEnumeration enum = attribute.getAll();
			while ((count > 0) && (enum.hasMore())) {
				Object obj = enum.next();
				if (obj.getClass().getName().equals(
				    "java.lang.String")) {
					if (fns_delete_attribute(name, attrID,
					    (String) obj) != 1) {
						throw new AMI_KeyMgntException(
						    "Error in FNS delete " +
						    "attribute: " +  attrID +
						    " " + (String) obj);
					} 
				} else if (fns_delete_binary_attribute(
				    name, attrID, (byte[]) obj) != 1) {
					throw new AMI_KeyMgntException(
					    "Error in FNS delete " +
					    "binary attribute: " +  attrID);
				}
			}
			if (count == 0) {
				if (fns_delete_attribute(name,
				    attrID, null) != 1)
					throw new AMI_KeyMgntException(
					    "Error in FNS delete attribute: " +
					     attrID);
			}
		} catch (NamingException e) {
			throw new AMI_KeyMgntException(e.toString());
		}
	}

	public void deleteAttributes(String name, Attributes attrs)
	    throws AMI_KeyMgntException {
		Attribute attribute;
		try {
			Attributes attrset =
			    getAttributesForNamingService(attrs);
			NamingEnumeration enum = attrset.getAll();
			while (enum.hasMore()) {
				attribute = (Attribute) enum.next();
				deleteAttribute(name, attribute);
			}
		} catch (NamingException e) {
			throw new AMI_KeyMgntException(e.toString());
		}
	}

	public Attribute getAttribute(String name, String attrID)
	    throws AMI_KeyMgntException {
		Attribute answer = null;
		String realAttrID = getAttributeIDForNamingService(attrID);
		Object[] attrValues = fns_get_attribute(name, realAttrID);
		if (attrValues == null)
			return null;
		Attribute attr = new BasicAttribute(attrID);
		for (int i = 0; i < attrValues.length; i++)
			attr.add(attrValues[i]);
		try {
			answer = getAttributeForApplication(attr);
		} catch (NamingException e) {
			throw new AMI_KeyMgntException(e.toString());
		}
		return (answer);
	}

	protected Attributes getSchemaDefinedAttributes(Attributes attrs)
	    throws AMI_KeyMgntException {
		String attrID;
		Attribute attribute;
		Attributes answer = new BasicAttributes();
		Enumeration enum = schema.getAllSchemaEntries();
		while (enum.hasMoreElements()) {
			attrID = (String) ((AMI_KeyMgntSchemaEntry)
			    enum.nextElement()).getAttributeID();
			if ((attribute = attrs.get(attrID)) != null)
				answer.put(attribute);
		}
		return (answer);
	}

	public Attributes getAttributes(String name)
	    throws AMI_KeyMgntException {
		String[] attrIDs = fns_get_attributeIDs(name);
		if (attrIDs == null)
			return null;
		Attributes attrset = new BasicAttributes();
		Attribute attr;
		for (int i = 0; i < attrIDs.length; i++) {
			attr = getAttribute(name, attrIDs[i]);
			if (attr != null)
				attrset.put(attr);
		}
		return (getSchemaDefinedAttributes(attrset));
	}


	public Enumeration list() throws AMI_KeyMgntException {
		String index, value, attrID, attrValue;

		index = new String(AMI_PROPERTY_PREFIX + amiPropertyIndex +
		    AMI_OBJECTCLASS);
		attrID = environment.getProperty(index, AMI_OBJECTCLASS);

		index = new String(AMI_PROPERTY_PREFIX + amiPropertyIndex +
		    AMI_OBJECTCLASS_VALUE);
		value = new String("AMI_KeyMgnt_" +
		    amiPropertyIndex.toUpperCase());
		attrValue = environment.getProperty(index, value);

		return (new AMI_KeyMgnt_FNS_Enumeration(
		    context, attrID, attrValue));
	}

	public Enumeration search(Attribute attribute)
	    throws AMI_KeyMgntException {
		String attrID, attrValue;
		try {
			attribute = getAttributeForNamingService(attribute);
			NamingEnumeration enum = attribute.getAll();
			if (enum == null)
				return null;
			attrID = attribute.getID();
			if (enum.hasMore())
				attrValue = (enum.next()).toString();
			else
				return null;
		} catch (NamingException e) {
			throw new AMI_KeyMgntException(e.toString());
		}

		String[] fns_list = fns_search(attrID, attrValue);
		if (fns_list == null)
			return (null);
		Vector answer = new Vector();
		for (int i = 0; i < fns_list.length; i++)
			answer.addElement(fns_list[i]);
		return (answer.elements());
	}

	protected void finalize() throws Throwable {
		fns_context_handle_destroy();
	}
}
