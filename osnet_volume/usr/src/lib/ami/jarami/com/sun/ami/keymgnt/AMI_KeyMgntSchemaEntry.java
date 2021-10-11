/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_KeyMgntSchemaEntry.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.keymgnt;

public class AMI_KeyMgntSchemaEntry extends Object {

	private String attributeID;
	private boolean isBinary;
	private boolean isSearchable;
	private boolean isMultiValued;
	private boolean isOwnerChangeable;
	private boolean isAdminChangeable;
	private boolean isEncrypted;

	public AMI_KeyMgntSchemaEntry(String attrID) {
		attributeID = new String(attrID);
		isBinary = false;
		isSearchable = false;
		isMultiValued = false;
		isOwnerChangeable = true;
		isAdminChangeable = true;
		isEncrypted = false;
	}

	public String getAttributeID() {
		return (new String(attributeID));
	}

	public void setBinary(boolean b) {
		isBinary = b;
	}

	public void setSearchable(boolean b) {
		isSearchable = b;
	}

	public void setMultivalued(boolean b) {
		isMultiValued = b;
	}

	public void setOwnerChangeable(boolean b) {
		isOwnerChangeable = b;
	}

	public void setAdminChangeable(boolean b) {
		isAdminChangeable = b;
	}

	public void setEncryption(boolean b) {
		isEncrypted = b;
	}

	public boolean isBinary() {
		return isBinary;
	}

	public boolean isSearchable() {
		return isSearchable;
	}

	public boolean isMultiValued() {
		return isMultiValued;
	}

	public boolean isSingleValued() {
		return (!isMultiValued);
	}

	public boolean isOwnerChangable() {
		return isOwnerChangeable;
	}

	public boolean isAdminChangeable() {
		return isAdminChangeable;
	}

	public boolean isEncrypted() {
		return isEncrypted;
	}

	public String toString() {
		String answer = new String(attributeID);
		if (isBinary())
			answer += "(binary,";
		else
			answer += "(ascii,";
		if (isSearchable())
			answer += "searchable,";
		if (isMultiValued())
			answer += "multivalued,";
		else
			answer += "single-valued,";
		if (isOwnerChangable())
			answer += "owner-changable,";
		if (isAdminChangeable())
			answer += "admin-changable,";
		if (isEncrypted())
			answer += "encrypted)";
		else
			answer += "non-encrypted)";
		return (answer);
	}

	public boolean equals(Object obj) {
		if ((obj != null) && (obj instanceof AMI_KeyMgntSchemaEntry)) {
			AMI_KeyMgntSchemaEntry entry =
			    (AMI_KeyMgntSchemaEntry) obj;
			return (attributeID.equalsIgnoreCase(
			    entry.attributeID));
		}
		return (false);
	}
}
