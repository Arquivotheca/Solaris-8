/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_KeyMgntSchema_Files_Enumeration.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.keymgnt;

import java.util.Enumeration;
import java.io.*;

/*
 * The format of the file is as follows: (# is for comments)
 * attrID,binary,searchable,multivalued,ownerchangeable,adminchangebale,\
 *	encrypted
 */
 


public class AMI_KeyMgntSchema_Files_Enumeration implements Enumeration {

	AMI_KeyMgnt_Files_Enumeration schemaElements;
	String remainingElement;

	public AMI_KeyMgntSchema_Files_Enumeration(String filename)
	    throws AMI_KeyMgntSchemaException {
		try {
			schemaElements =
			    new AMI_KeyMgnt_Files_Enumeration(filename);
		} catch (AMI_KeyMgntException e) {
			throw new AMI_KeyMgntSchemaException(e.toString());
		}
		remainingElement = null;
	}

	public boolean hasMoreElements() {
		return (schemaElements.hasMoreElements());
	}

	public Object nextElement() {
		if (!schemaElements.hasMoreElements())
			return null;

		// Get the attribute ID
		String attrID = null;
		if (remainingElement != null)
			attrID = remainingElement;
		else
			attrID = (String) schemaElements.nextElement();
		if (attrID == null)
			return (null);
		AMI_KeyMgntSchemaEntry answer =
		    new AMI_KeyMgntSchemaEntry(attrID);

		// Check if attribute is binary
		remainingElement = null;
		String attrSyntax = (String) schemaElements.nextElement();
		if (attrSyntax == null)
			return (answer);
		if ((attrSyntax.equalsIgnoreCase("false") == false) &&
		    (attrSyntax.equalsIgnoreCase("true") == false)) {
			remainingElement = new String(attrSyntax);
			return (answer);
		}
		if (attrSyntax.equalsIgnoreCase("true"))
			answer.setBinary(true);

		// Check if attribute is searchable
		attrSyntax = (String) schemaElements.nextElement();
		if (attrSyntax == null)
			return (answer);
		if ((attrSyntax.equalsIgnoreCase("false") == false) &&
		    (attrSyntax.equalsIgnoreCase("true") == false)) {
			remainingElement = new String(attrSyntax);
			return (answer);
		}
		if (attrSyntax.equalsIgnoreCase("true"))
			answer.setSearchable(true);

		// Check if attribute is Multivalued
		attrSyntax = (String) schemaElements.nextElement();
		if (attrSyntax == null)
			return (answer);
		if ((attrSyntax.equalsIgnoreCase("false") == false) &&
		    (attrSyntax.equalsIgnoreCase("true") == false)) {
			remainingElement = new String(attrSyntax);
			return (answer);
		}
		if (attrSyntax.equalsIgnoreCase("true"))
			answer.setMultivalued(true);

		// Check if attribute is OwnerChangeable
		attrSyntax = (String) schemaElements.nextElement();
		if (attrSyntax == null)
			return (answer);
		if ((attrSyntax.equalsIgnoreCase("false") == false) &&
		    (attrSyntax.equalsIgnoreCase("true") == false)) {
			remainingElement = new String(attrSyntax);
			return (answer);
		}
		if (attrSyntax.equalsIgnoreCase("false"))
			answer.setOwnerChangeable(false);

		// Check if attribute is AdminChangeable
		attrSyntax = (String) schemaElements.nextElement();
		if (attrSyntax == null)
			return (answer);
		if ((attrSyntax.equalsIgnoreCase("false") == false) &&
		    (attrSyntax.equalsIgnoreCase("true") == false)) {
			remainingElement = new String(attrSyntax);
			return (answer);
		}
		if (attrSyntax.equalsIgnoreCase("false"))
			answer.setAdminChangeable(false);

		// Check if attribute is Encrypted
		attrSyntax = (String) schemaElements.nextElement();
		if (attrSyntax == null)
			return (answer);
		if ((attrSyntax.equalsIgnoreCase("false") == false) &&
		    (attrSyntax.equalsIgnoreCase("true") == false)) {
			remainingElement = new String(attrSyntax);
			return (answer);
		}
		if (attrSyntax.equalsIgnoreCase("true"))
			answer.setEncryption(true);

		// Check if there are more elements
		attrSyntax = (String) schemaElements.nextElement();
		if (attrSyntax != null)
			remainingElement = new String(attrSyntax);

		return (answer);
	}
}
