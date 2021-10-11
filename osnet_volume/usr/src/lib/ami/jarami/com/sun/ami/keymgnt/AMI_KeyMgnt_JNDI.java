/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_KeyMgnt_JNDI.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.keymgnt;

import java.lang.*;
import java.util.*;
import javax.naming.*;
import javax.naming.directory.*;

public class AMI_KeyMgnt_JNDI extends AMI_KeyMgnt {

	// Properties definitions for JNDI
	protected final static String JNDI_PREFIX = "java.naming.";
	protected final static String JNDI_FACTORY_INITIAL =
	   "java.naming.factory.initial";
	protected final static String JNDI_PROVIDER_URL =
	   "java.naming.provider.url";

	// Properties specfic to AMI
	protected final static String JNDI_DIRECTORY_ROOT_SUFFIX =
	   "directoryRoot";

	DirContext jndiContext;
	DirContext jndiInitContext;
	NameParser jndiNameParser;
	Name dirRootName;
	
	protected static String removeQuotes(String nameDN) {
		if (nameDN == null)
			return (null);

		if (nameDN.charAt(0) == '\"') {
			nameDN = new String(nameDN.substring(1));
		}
		if (nameDN.charAt(nameDN.length()-1) == '\"') {
			nameDN = new String(nameDN.substring(0,
			    nameDN.length()-1));
		}
		return (nameDN);
	}

	public AMI_KeyMgnt_JNDI(AMI_KeyMgntSchema ischema, Properties env)
	    throws AMI_KeyMgntException {
		super(ischema, env);

		// Generate JNDI specific properties
		String id, value, amiPrefix, jndiId;
		Properties jndiProperties = new Properties();
		amiPrefix = new String(AMI_PROPERTY_PREFIX +
		    amiPropertyIndex);
		Enumeration enum = environment.keys();
		while (enum.hasMoreElements()) {
			id = (String) enum.nextElement();
			if (id.startsWith(amiPrefix, 0)) {
				jndiId = new String(JNDI_PREFIX +
				    id.substring(amiPrefix.length()));
				jndiProperties.put(jndiId,
				    removeQuotes(environment.getProperty(id)));
			}
		}

		// Check if the required properties are
		// defined in environment properties
		if ((jndiProperties.getProperty(
		    JNDI_FACTORY_INITIAL) == null) ||
		    (jndiProperties.getProperty(
		    JNDI_PROVIDER_URL) == null)) {
			throw new AMI_KeyMgntPropertyNotFoundException(
	  		    "Required JNDI environment " +
			    "properties NOT defined");
		}

		// Add environment property for binary attribtues
		String binaryAttributes = null;
		Enumeration senum = ischema.getAllSchemaEntries();
		AMI_KeyMgntSchemaEntry entry;
		while (senum.hasMoreElements()) {
			entry = (AMI_KeyMgntSchemaEntry) senum.nextElement();
			if (entry.isBinary()) {
				if (binaryAttributes == null)
					binaryAttributes = new String(
					getAttributeIDForNamingService(
					entry.getAttributeID().toLowerCase()));
				else
					binaryAttributes += " " +
					getAttributeIDForNamingService(
					entry.getAttributeID().toLowerCase());
			}
		}
		if (binaryAttributes != null) {
			jndiProperties.put(
			    "java.naming.ldap.attributes.binary",
			    binaryAttributes);
		}
		
		// Get the initial context
		jndiInitContext = null;
		try {
			jndiInitContext = new InitialDirContext(jndiProperties);
			if (jndiInitContext == null)
				throw new AMI_KeyMgntException(
				    "Unable to get initial context");
		} catch (NamingException e) {
			convertNamingExceptionToAMIException(e);
		}

		// Determine the directory root
		String dirRoot = environment.getProperty(AMI_PROPERTY_PREFIX +
		    amiPropertyIndex + JNDI_DIRECTORY_ROOT_SUFFIX);
		if (dirRoot == null) {
			// Search and find the dirRoot, LDAP specific
			Attributes matchAttrs = new BasicAttributes(
			    "objectClass", "top");
			try {
				NamingEnumeration enumN =
				    jndiInitContext.search("", matchAttrs);
				if ((enumN != null) && (enumN.hasMore())) {
					SearchResult sr = (SearchResult)
					    enumN.next();
					dirRoot = (String) sr.getName();
				}
			} catch (NamingException f) {
				convertNamingExceptionToAMIException(f);
			}
		}

		// Reset the initial context to the specified organization
		try {
			if (dirRoot != null) {
				jndiContext = (DirContext)
				    jndiInitContext.lookup(dirRoot);
	
				jndiNameParser = jndiContext.getNameParser("");
				dirRootName = jndiNameParser.parse(dirRoot);
			} else {
				jndiContext = jndiInitContext;
				jndiNameParser = null;
				dirRootName = null;
			}
		} catch (NamingException g) {
			convertNamingExceptionToAMIException(g);
		}
	}

	protected boolean attributeContains(Attribute attr, String attrValue)
	    throws NamingException {
		NamingEnumeration enum = attr.getAll();
		String value;
		while (enum.hasMoreElements()) {
			value = (String) enum.nextElement();
			if (value.equalsIgnoreCase(attrValue))
				return (true);
		}
		return (false);
	}

	protected Attributes checkAndAddAttribute(String name, String attrID,
	    Attributes attrset) throws NamingException {
		Attributes attrs = null;
		String ids[] = new String[1];
		ids[0] = attrID;
		try {
			if ((jndiNameParser != null) &&
			    (jndiNameParser.parse(name).startsWith(
			    dirRootName)))
				attrs = jndiInitContext.getAttributes(
				    name, ids);
			else
				attrs = jndiContext.getAttributes(name, ids);
		} catch (NameNotFoundException nnfe) {
		}

		if ((attrs != null) && (attrs.size() > 0))
			return (attrset);

		// Added an empty attribute
		attrset.put(new BasicAttribute(attrID, ""));
		return (attrset);
	}

	protected Attributes addAdditionalAttributes(String name,
	    Attributes attrset) throws NamingException {
		String propIndex = new String(AMI_PROPERTY_PREFIX +
		    amiPropertyIndex + AMI_ADDITIONAL_ATTR);
		String additionalAttrs = environment.getProperty(propIndex);
		if (additionalAttrs == null)
			return (attrset);

		StringTokenizer tokens = new StringTokenizer(additionalAttrs,
		    ",");
		while (tokens.hasMoreTokens()) {
			String attrID = tokens.nextToken();
			attrset = checkAndAddAttribute(name, attrID, attrset);
		}
		return (attrset);
	}

	protected Attributes checkAndAddObjectClass(String name, String attrID,
	    String objectClass, Attributes attrset) throws NamingException {
		Attributes attrs = null;
		String ids[] = new String[1];
		ids[0] = attrID;
		try {
			if ((jndiNameParser != null) &&
			    (jndiNameParser.parse(name).startsWith(
			    dirRootName)))
				attrs = jndiInitContext.getAttributes(
				    name, ids);
			else
				attrs = jndiContext.getAttributes(name, ids);
		} catch (NameNotFoundException nnfe) {
		}

		Attribute ocAttr = null;
		if (attrs != null)
			ocAttr = attrs.get(attrID);
		if ((ocAttr != null) && attributeContains(ocAttr, objectClass))
			return (attrset);

		// Added object class
		ocAttr = attrset.get(attrID);
		if (ocAttr == null)
			ocAttr = new BasicAttribute(attrID);
		ocAttr.add(objectClass);
		attrset.put(ocAttr);
		return (attrset);
	}

	protected Attributes addObjectClassAttribute(String name,
	    Attributes attrset) throws AMI_KeyMgntException, NamingException {
		// Get the object class attribute ID
		String attrIdIndex = new String(AMI_PROPERTY_PREFIX +
		    amiPropertyIndex + AMI_OBJECTCLASS);
		String attrID = environment.getProperty(attrIdIndex,
		    AMI_OBJECTCLASS);

		// Get the object class attribute value
		String attrValueIndex = new String(AMI_PROPERTY_PREFIX +
		    amiPropertyIndex +  AMI_OBJECTCLASS_VALUE);
		String value = new String("AMI_KeyMgnt_" +
		    amiPropertyIndex.toUpperCase());
		String attrValue = environment.getProperty(
		    attrValueIndex, value);

		attrset = checkAndAddObjectClass(name, attrID,
		    attrValue, attrset);

		// Add additional object classes
		String propIndex = new String(AMI_PROPERTY_PREFIX +
		    amiPropertyIndex + AMI_ADDITIONAL_OC);
		String additionalOCs = environment.getProperty(propIndex);
		if (additionalOCs != null) {
			StringTokenizer tokens =
			    new StringTokenizer(additionalOCs, ",");
			while (tokens.hasMoreTokens()) {
				String attrOC = tokens.nextToken();
				attrset = checkAndAddObjectClass(name, attrID,
				    attrOC, attrset);
			}
		}
		return (addAdditionalAttributes(name, attrset));
	}

	protected void convertNamingExceptionToAMIException(NamingException ne)
	    throws AMI_KeyMgntException {
		if (ne instanceof CommunicationException)
			throw new AMI_KeyMgntCommunicationException(
			    ne.toString());
		else
			throw new AMI_KeyMgntException(ne.toString());
	}

	public void create(String inName, Attributes attrset)
	    throws AMI_KeyMgntException {
		try {
			// %%% Need to do schema checking here
			// ie., check if ami objectclass is present,
			// if not convert AMI_KeyMgntSchema to Attributes
			// and store them as ami objectclass

			// If inName is not provided, derive it from
			// the attribtues
			String name = getIndexName(inName, attrset);
			attrset = getAttributesForNamingService(attrset);
			attrset = addObjectClassAttribute(name, attrset);

			if ((jndiNameParser != null) &&
			    (jndiNameParser.parse(name).startsWith(
			    dirRootName))) {
				jndiInitContext.createSubcontext(name, attrset);
			} else {
				jndiContext.createSubcontext(name, attrset);
			}
		} catch (NamingException e) {
			convertNamingExceptionToAMIException(e);
		}
	}

	public void destroy(String name) throws AMI_KeyMgntException {
		try {
			if ((jndiNameParser != null) &&
			    (jndiNameParser.parse(name).startsWith(
			    dirRootName)))
				jndiInitContext.destroySubcontext(name);
			else
				jndiContext.destroySubcontext(name);
		} catch (NamingException e) {
			convertNamingExceptionToAMIException(e);
		}
	}

	public void addAttributes(String inName, Attributes attrset)
	    throws AMI_KeyMgntException {
		try {
			String name = getIndexName(inName, attrset);
			attrset = getAttributesForNamingService(attrset);
			// attrset = addObjectClassAttribute(name, attrset);

			if ((jndiNameParser != null) &&
			    (jndiNameParser.parse(name).startsWith(
			    dirRootName)))
				jndiInitContext.modifyAttributes(name,
				    DirContext.ADD_ATTRIBUTE, attrset);
			else
				jndiContext.modifyAttributes(name,
				    DirContext.ADD_ATTRIBUTE, attrset);
		} catch (NamingException e) {
			convertNamingExceptionToAMIException(e);
		}
	}

	public void deleteAttributes(String name, Attributes attrset)
	    throws AMI_KeyMgntException {
		try {
			attrset = getAttributesForNamingService(attrset);
			if ((jndiNameParser != null) &&
			    (jndiNameParser.parse(name).startsWith(
			    dirRootName))) {
				jndiInitContext.modifyAttributes(name,
				    DirContext.REMOVE_ATTRIBUTE, attrset);
			} else {
				jndiContext.modifyAttributes(name,
				    DirContext.REMOVE_ATTRIBUTE, attrset);
			}
		} catch (NoSuchAttributeException noe) {
			// doNothing
		} catch (NamingException e) {
			convertNamingExceptionToAMIException(e);
		}
	}
		
	public Attributes getAttributes(String name)
	    throws AMI_KeyMgntException {
		Attributes answer = null;

		// Construct the list for selected attributes
		String attrId[] = new String[schema.getNumSchemaEntries()];
		Enumeration enum = schema.getAllSchemaEntries();
		String idIndexPrefix = new String(AMI_PROPERTY_PREFIX +
		    amiPropertyIndex);
		String id, idIndex;
		int i = 0;
		while (enum.hasMoreElements()) {
			id = (String) ((AMI_KeyMgntSchemaEntry)
			    enum.nextElement()).getAttributeID();
			idIndex = new String(idIndexPrefix + id);
			attrId[i++] = environment.getProperty(idIndex, id);
		}
		try {
			if ((jndiNameParser != null) &&
			    (jndiNameParser.parse(name).startsWith(
			    dirRootName)))
				answer = jndiInitContext.getAttributes(
				    name, attrId);
			else
				answer = jndiContext.getAttributes(name,
				    attrId);
			if (answer != null)
				answer = getAttributesForApplication(answer);
		} catch (NamingException e) {
			convertNamingExceptionToAMIException(e);
		}
		return (answer);
	}

	public Attribute getAttribute(String name, String attrID)
	    throws AMI_KeyMgntException {
		Attribute answer = null;
		try {
			Attributes attrs;
			String attrIndex = new String(AMI_PROPERTY_PREFIX +
			    amiPropertyIndex + attrID);
			String realAttrID = environment.getProperty(
			    attrIndex, attrID);
			String ids[] = new String[1];
			ids[0] = realAttrID;
			if ((jndiNameParser != null) &&
			    (jndiNameParser.parse(name).startsWith(
			    dirRootName)))
				attrs = jndiInitContext.getAttributes(name,
				    ids);
			else
				attrs = jndiContext.getAttributes(name, ids);
			if (attrs != null)
				answer = attrs.get(realAttrID);
			if (answer != null)
				getAttributeForApplication(answer);
		} catch (NamingException e) {
			convertNamingExceptionToAMIException(e);
		}
		return (answer);
	}

	public Enumeration list() throws AMI_KeyMgntException {
		NamingEnumeration enum = null;
		try {
			String id, value, idIndex, valueIndex;
			idIndex = new String(AMI_PROPERTY_PREFIX +
			    amiPropertyIndex + AMI_OBJECTCLASS);
			id = environment.getProperty(idIndex,
			    AMI_OBJECTCLASS);
			valueIndex = new String(AMI_PROPERTY_PREFIX +
			    amiPropertyIndex.toLowerCase() +
			    AMI_OBJECTCLASS_VALUE);
			value = new String("AMI_KeyMgnt_" +
			    amiPropertyIndex.toUpperCase());
			value = environment.getProperty(valueIndex, value);
			Attributes attrs = new BasicAttributes(id, value);
			enum = jndiContext.search("", attrs);
		} catch (NamingException e) {
			convertNamingExceptionToAMIException(e);
		}
		if (enum != null)
			return (new AMI_KeyMgnt_JNDI_Enumeration(enum));
		else
			return (null);
	}

	public Enumeration search(Attribute attribute)
	    throws AMI_KeyMgntException {
		NamingEnumeration answer = null;
		try {
			Attributes attrs = new BasicAttributes();
			attrs.put(getAttributeForNamingService(attribute));
			answer = jndiContext.search("", attrs);
		} catch (NamingException e) {
			convertNamingExceptionToAMIException(e);
		}
		if (answer != null)
			return (new AMI_KeyMgnt_JNDI_Enumeration(answer));
		else
			return (null);
	}
}

class AMI_KeyMgnt_JNDI_Enumeration implements Enumeration {
	NamingEnumeration enum;

	AMI_KeyMgnt_JNDI_Enumeration(NamingEnumeration e) {
		enum = e;
	}

	public boolean hasMoreElements() {
		try {
			return (enum.hasMore());
		} catch (NamingException e) {
			return (false);
		}
	}

	public Object nextElement() {
		NameClassPair ncp = null;
		try {
			ncp = (NameClassPair) enum.next();
		} catch (NamingException e) {
		}
		return (ncp.getName());
	}
}
