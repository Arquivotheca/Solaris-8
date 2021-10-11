/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)AMI_KeyMgnt_FILE.java	1.1 99/07/11 SMI"
 *
 */

package com.sun.ami.keymgnt;

import java.util.Enumeration;
import java.util.Properties;
import java.util.Vector;
import java.io.*;
import java.lang.*;
import javax.naming.*;
import javax.naming.directory.*;
import com.sun.ami.keygen.AMI_VirtualHost;

public class AMI_KeyMgnt_FILE extends AMI_KeyMgnt {

	protected static final String USER_PROFILE_FILE = ".amiprofile";
	protected static final String ROOT_KEYS_DIRECTORY = "/etc/ami/keys";
	protected static final String USER_INDEX_ATTR_ID = "nameuser";
	protected static final String HOST_INDEX_ATTR_ID = "namehost";
	protected AMI_KeyMgntSchema schema;

	/* Native method to get the home directory of a user */
	native String ami_get_user_home_directory(String name);

	public AMI_KeyMgnt_FILE(AMI_KeyMgntSchema ischema, Properties env)
	    throws AMI_KeyMgntException {
		super(ischema, env);
		schema = new AMI_KeyMgntSchema(ischema);
	}

	// Format of the binary data file
	// <version><integer of number of items>
	// <length of first item><first binary item>
	// <length of second item><second binary item>
	// ...
	protected Vector readFileObjects(String attrid, String filename)
	    throws AMI_KeyMgntException {
		Vector answer = new Vector();
		File file = new File(filename);
		if (!file.exists() || !file.canRead())
			return (answer);

		FileInputStream fis = null;
		try {
			fis = new FileInputStream(file);
		} catch (FileNotFoundException e) {
			throw new AMI_KeyMgntException(
			    "File not found: " + filename);
		}
		if (attrid.startsWith("keystore")) {
			byte[] keystore = new byte[(int) file.length()];
			try {
				if (fis.read(keystore) != keystore.length)
					throw new AMI_KeyMgntException(
					   "Unable to read the file: " +
					    filename);
			} catch (IOException io) {
				throw new AMI_KeyMgntException(
				   "Unable to read the file: " + filename);
			}
			answer.add(keystore);
			return (answer);
		}

		try {
			DataInputStream dis = new DataInputStream(fis);
			int version = dis.readInt();
			int numItems = dis.readInt();
			int dataSize;
			byte[] data;
			for (int i = 0; i < numItems; i++) {
				dataSize = dis.readInt();
				data = new byte[dataSize];
				dis.readFully(data);
				answer.add(data);
			}
		} catch (IOException ioe) {
			throw new AMI_KeyMgntException(
			   "Unable to read the file: " + filename);
		}
		return (answer);
	}

	protected void writeFileObjects(String attrid, Vector objs,
	    String filename) throws AMI_KeyMgntException {
		File file = new File(filename);

		// First delete the file
		file.delete();
		if (objs.size() == 0)
			return;

		FileOutputStream fos = null;
		try {
			fos = new FileOutputStream(file);
		} catch (IOException io) {
			throw new AMI_KeyMgntException(
			   "Unable to open file for writting: " + filename);
		}
		if (attrid.startsWith("keystore")) {
			byte[] keystore = (byte[]) objs.elementAt(0);
			try {
				fos.write(keystore);
				fos.close();
			} catch (IOException io) {
				throw new AMI_KeyMgntException(
				   "Unable to write to file: " + filename);
			}
			return;
		}

		try {
			// DataOutputStream
			DataOutputStream dos = new DataOutputStream(fos);
			dos.writeInt(1); // Version
			dos.writeInt(objs.size()); // Number of items
			byte[] data;
			Enumeration enum = objs.elements();
			while (enum.hasMoreElements()) {
				data = (byte[]) enum.nextElement();
				dos.writeInt(data.length);
				dos.write(data, 0, data.length);
			}
			fos.close();
		} catch (IOException ioe) {
			throw new AMI_KeyMgntException(
			   "Unable to write to the file: " + filename);
		}
	}

	// Method to create the ROOT Keys directory
	protected void createRootKeysDirectory() {
		File keysDirectory = new File(ROOT_KEYS_DIRECTORY);
		if (!keysDirectory.exists()) {
			// Create the directory
			keysDirectory.mkdirs();
			// Change permissions
			// %%% TBD
		}
	}
		
	// Method to obtain the file name, given the attribute ID
	protected String getFileName(String name, AMI_UserProfile profile,
	    String attrID) throws AMI_KeyMgntException {
		// Construct the index name into the AMI profile
		String indexName = null;
		if (attrID.startsWith("keystore"))
			indexName = new String("keystorefile");
		else
			indexName = new String(attrID.toLowerCase() + "file");

		// Get the user defined file name from the profile
		String fileName = profile.getProperty(indexName);
		if (fileName != null)
			return (fileName);

		// Obtain the default file name
		String defaultFile = null;
		if ((name.indexOf('.') != -1) ||
		    (AMI_KeyMgnt_FNS.fns_get_euid(name) == 0)) {
			if (AMI_KeyMgnt_FNS.fns_get_euid(name) == 0)
				name = AMI_VirtualHost.getHostIP();
			createRootKeysDirectory();
			if (attrID.startsWith("keystore")) {
				defaultFile = new String(
				    ROOT_KEYS_DIRECTORY +
				    File.separator + name +
				    ".keystore");
			} else {
				defaultFile = new String(ROOT_KEYS_DIRECTORY +
				    File.separator + name +
				    "." + attrID.toLowerCase());
			}
		} else {
			String homeDir = ami_get_user_home_directory(name);
			if (attrID.startsWith("keystore")) {
				defaultFile = new String(homeDir +
				    File.separator + ".keystore");
			} else {
				defaultFile = new String(homeDir +
				    File.separator + "." +
				    attrID.toLowerCase());
			}
		}
		return (defaultFile);
	}

	// Method to get the AMI Profile file name
	protected File getUserProfileFile(String name)
	    throws AMI_KeyMgntException {
		// Obtain the User Profile filename
		String profileFile = null;
		if ((name.indexOf('.') != -1) ||
		    (AMI_KeyMgnt_FNS.fns_get_euid(name) == 0)) {
			if (AMI_KeyMgnt_FNS.fns_get_euid(name) == 0)
				name = AMI_VirtualHost.getHostIP();
			createRootKeysDirectory();
			profileFile = new String(ROOT_KEYS_DIRECTORY +
				    File.separator + name +
				    USER_PROFILE_FILE);
		} else {
			String homeDir = ami_get_user_home_directory(name);
			profileFile = new String(homeDir +
			    File.separator + USER_PROFILE_FILE);
		}

		// Construct the file object
		File userProfileFile = new File(profileFile);
		return (userProfileFile);
	}

	protected void setUserProfile(String name, AMI_UserProfile profile)
	    throws AMI_KeyMgntException {
		// Make sure the "name" is same as the user name
		String username = System.getProperty("user.name");
		if ((!username.equalsIgnoreCase(name)) &&
		    (!name.equals(AMI_VirtualHost.getHostIP())))
			throw new AMI_KeyMgntException(
			    "Invalid index name: " + name);

		File userProfileFile = getUserProfileFile(name);
		userProfileFile.delete();
		try {
			FileOutputStream userProfileStream =
			    new FileOutputStream(userProfileFile);
			userProfileStream.write(profile.toByteArray());
			userProfileStream.close();
		} catch (Exception io) {
			throw new AMI_KeyMgntException(
			   "Unable to write to the User Profile file");
		}
	}

	protected AMI_UserProfile getUserProfile(String name)
	    throws AMI_KeyMgntException {
		File userProfileFile = getUserProfileFile(name);
		if (!userProfileFile.exists()) {
			AMI_UserProfile answer = new AMI_UserProfile();
			return (answer);
		}
		if (!userProfileFile.canRead())
			throw new AMI_KeyMgntException(
			    "Unable to read user profile file: " +
			    userProfileFile);

		try {
			FileInputStream userProfileStream =
			    new FileInputStream(userProfileFile);
			byte[] serializedUserProfile =
			    new byte[(int) userProfileFile.length()];
			if (userProfileStream.read(serializedUserProfile) !=
			    serializedUserProfile.length)
				throw new AMI_KeyMgntException(
				    "Unable to read the User Profile File: " +
				    userProfileFile);
			userProfileStream.close();
			return (new AMI_UserProfile(serializedUserProfile));
		} catch (Exception e) {
			throw new AMI_KeyMgntException(
			   "Unable to read Profile file");
		}
	}

	// Create an AMI Object
	public void create(String name, Attributes attrset)
	    throws AMI_KeyMgntException {
		Attribute attribute;
		String id, value;

		// Add the attributes to the AMI User Profile
		AMI_UserProfile profile = getUserProfile(name);
		Enumeration attrs = attrset.getAll();
		while (attrs.hasMoreElements()) {
			attribute = (Attribute) attrs.nextElement();
			id = attribute.getID().toLowerCase();
			try {
				value = (String) attribute.get();
			} catch (NamingException ne) {
				throw new AMI_KeyMgntException(
				    ne.getMessage());
			}
			profile.setIndexNameAttribute(id, value);
		}
		setUserProfile(name, profile);
	}

	public void destroy(String name) throws AMI_KeyMgntException {
		// Remove all the AMI related files and AMI User profile
		// %%% TBD
	}

	protected boolean isIndexName(String attrid)
	    throws AMI_KeyMgntException {
		AMI_KeyMgntSchemaEntry entry =
		    schema.getAttributeSchema(attrid);
		if (entry == null)
			return (false);
		return (entry.isSearchable());
	}

	protected void addAttribute(String name, Attribute attribute)
	    throws AMI_KeyMgntException {
		AMI_UserProfile profile = getUserProfile(name);
		String attrid = attribute.getID().toLowerCase();
		if (isIndexName(attrid)) {
			// Add it to the user profile
			String value = null;
			try {
				value = (String) attribute.get();
			} catch (NamingException ne) {
				throw new AMI_KeyMgntException(
				    ne.getMessage());
			}
			if (value == null)
				profile.removeIndexNameAttribute(attrid);
			else
				profile.setIndexNameAttribute(attrid, value);
			setUserProfile(name, profile);
			return;
		} else if (attrid.equalsIgnoreCase("objectprofile")) {
			// It is the user profile itself
			byte[] serializedProfile = null;
			try {
				serializedProfile = (byte[]) attribute.get();
			} catch (NamingException n) {
				throw new AMI_KeyMgntException(n.getMessage());
			}
			try {
				AMI_UserProfile newProfile =
				    new AMI_UserProfile(serializedProfile);
				profile.merge(newProfile);
			} catch (Exception e) {
				throw new AMI_KeyMgntException(e.getMessage());
			}
			setUserProfile(name, profile);
			return;
		}

		// Get the attibute list and add the new attribute
		String fileName = getFileName(name, profile, attrid);
		Vector values = readFileObjects(attrid, fileName);
		try {
			Enumeration enum = attribute.getAll();
			while (enum.hasMoreElements())
				values.add(enum.nextElement());
		} catch (NamingException ine) {
			throw new AMI_KeyMgntException(
			    ine.getMessage());
		}
		writeFileObjects(attrid, values, fileName);
	}

	public void addAttributes(String iname, Attributes attrset)
	    throws AMI_KeyMgntException {
		// Obtain the individual attributes and add them
		Enumeration enum = attrset.getAll();
		while (enum.hasMoreElements())
			addAttribute(iname, (Attribute) enum.nextElement());
	}

	protected void deleteAttribute(String name, Attribute attribute)
	    throws AMI_KeyMgntException {
		AMI_UserProfile profile = getUserProfile(name);
		String attrid = attribute.getID().toLowerCase();
		if (isIndexName(attrid)) {
			// Remove it from the user profile
			profile.removeIndexNameAttribute(attrid);
			setUserProfile(name, profile);
			return;
		} else if (attrid.equalsIgnoreCase("objectprofile")) {
			// Remove all non-index name attributes
			profile.removeNonIndexNameAttributes();
			setUserProfile(name, profile);
			return;
		}

		// Get the attibute list and delete the attributes
		String fileName = getFileName(name, profile, attrid);
		Vector values = readFileObjects(attrid, fileName);
		if (attribute.size() == 0) {
			// Remove all attributes
			values.removeAllElements();
			writeFileObjects(attrid, values, fileName);
			return;
		}

		// Iterate through the attribute list and remove the
		// the attributes. Since, the vector elements are byte[]
		// Vector.remove() does not work.
		Vector newvalues;
		byte[] attrvalue, value;
		String attrstring, valstring;
		Enumeration venum = null, nenum = null;
		try {
			nenum = attribute.getAll();
		} catch (NamingException ne) {
			throw new AMI_KeyMgntException(
			    ne.getMessage());
		}

		while (nenum.hasMoreElements()) {
			attrvalue = (byte[]) nenum.nextElement();
			attrstring = new String(attrvalue);
			newvalues = new Vector();
			venum = values.elements();
			while (venum.hasMoreElements()) {
				value = (byte[]) venum.nextElement();
				valstring = new String(value);
				if (!valstring.equals(attrstring))
					newvalues.add(value);
			}
			values = newvalues;
		}
		writeFileObjects(attrid, values, fileName);
	}

	public void deleteAttributes(String name, Attributes attrs)
	    throws AMI_KeyMgntException {
		// Obtain individual attributes and delete them
		Enumeration enum = attrs.getAll();
		while (enum.hasMoreElements())
			deleteAttribute(name, (Attribute) enum.nextElement());
	}

	public Attribute getAttribute(String name, String attrID)
	    throws AMI_KeyMgntException {
		attrID = attrID.toLowerCase();
		Attribute answer = new BasicAttribute(attrID);
		AMI_UserProfile profile = getUserProfile(name);
		if (isIndexName(attrID)) {
			String value = profile.getIndexNameAttribute(attrID);
			if (value != null) {
				answer.add(value);
				return (answer);
			} else
				return (null);
		} else if (attrID.equalsIgnoreCase("objectprofile")) {
			try {
				answer.add(profile.toByteArray());
			} catch (Exception e) {
				throw new AMI_KeyMgntException(e.getMessage());
			}
			return (answer);
		}

		String fileName = getFileName(name, profile, attrID);
		Vector values = readFileObjects(attrID, fileName);
		if (values.size() == 0)
			return (null);

		Enumeration enum = values.elements();
		while (enum.hasMoreElements())
			answer.add(enum.nextElement());
		return (answer);
	}

	public Attributes getAttributes(String name)
	    throws AMI_KeyMgntException {
		// Get all the attribute
		Attributes answer = new BasicAttributes();
		Enumeration enum = schema.getAllSchemaEntries();
		AMI_KeyMgntSchemaEntry entry;
		String attrid;
		Attribute attribute;
		while (enum.hasMoreElements()) {
			entry = (AMI_KeyMgntSchemaEntry) enum.nextElement();
			attrid = entry.getAttributeID();
			attribute = new BasicAttribute(attrid,
			    getAttribute(name, attrid));
			answer.put(attribute);
		}
		return (answer);
	}

	public Enumeration list() throws AMI_KeyMgntException {
		// Return only the current user
		Vector answer = new Vector();
		String name = System.getProperty("user.name");
		if (AMI_KeyMgnt_FNS.fns_get_euid(null) == 0)
			answer.add(AMI_VirtualHost.getHostIP());
		else
			answer.add(name);
		return (answer.elements());
	}

	public Enumeration search(Attribute attribute)
	    throws AMI_KeyMgntException {
		// Return only the current user if the attribute matches
		// or if the attrID is "nameuser" or "namehost"
		String attrID = attribute.getID().toLowerCase();
		String inValue = null;
		try {
			inValue = (String) attribute.get();
		} catch (NamingException ne) {
			throw new AMI_KeyMgntException(
			    ne.getMessage());
		}

		// If serach is for nameuser or namehost, return the value
		if (attrID.equalsIgnoreCase(USER_INDEX_ATTR_ID) ||
		    attrID.equalsIgnoreCase(HOST_INDEX_ATTR_ID)) {
			// Make sure the object exists ie., profileFile exists
			File profileFile = getUserProfileFile(inValue);
			if (!profileFile.exists())
				return (null);
			Vector answer = new Vector();
			answer.add(inValue);
			return (answer.elements());
		}

		// Search within the "thisuser's" profile
		String name = System.getProperty("user.name");
		AMI_UserProfile profile = getUserProfile(name);
		String value = profile.getIndexNameAttribute(attrID);
		if (value == null)
			return (null);

		if ((inValue != null) && (inValue.equalsIgnoreCase(value)))
			return (list());
		else
			return (null);
	}
}
