/*
 * @(#)Option.java	1.6 99/07/30 SMI
 *
 * Copyright (c) 1998-1999 Sun Microsystems, Inc.
 * All rights reserved.
 */

package com.sun.dhcpmgr.data;

import java.util.*;
import java.io.Serializable;

/**
 * Option is a simple data class which encapsulates an option record in
 * the dhcptab.  See dhcptab(4) for the gory details on options (aka symbols).
 *
 * @see DhcptabRecord
 * @see Macro
 */
public class Option extends DhcptabRecord implements Serializable, Cloneable {
    private byte context;
    private short code;
    private byte type;
    private int granularity;
    private int maximum;
    private Vector vendors;
    private boolean valueClean = false;
    private boolean validKey = true;
    private boolean validValue = true;

    // Definitions for context values
    public static final byte STANDARD = 0;
    public static final byte EXTEND = 1;
    public static final byte SITE = 2;
    public static final byte VENDOR = 3;
    public static final byte CONTEXTS = 4;
    
    // Definitions for type values
    public static final byte ASCII = 0;
    public static final byte BOOLEAN = 1;
    public static final byte IP = 2;
    public static final byte NUMBER = 3;
    public static final byte OCTET = 4;
    public static final byte TYPES = 5;
    
    // Definitions for ranges for the various contexts
    private static final short STANDARD_BEGIN = 1;
    private static final short STANDARD_END = 76;
    private static final short EXTEND_BEGIN = 77;
    private static final short EXTEND_END = 127;
    private static final short SITE_BEGIN = 128;
    private static final short SITE_END = 254;
    private static final short VENDOR_BEGIN = 1;
    private static final short VENDOR_END = 254;
    private static final short SPECIAL_BEGIN = 257;
    private static final short SPECIAL_END = 1026;
    
    // The string used in the dhcptab for contexts and types
    private static final String [] ctxts = { "", "Extend", "Site", "Vendor=" };
    private static final String [] types = { "ASCII", "BOOLEAN", "IP",
	"NUMBER", "OCTET" };
    
    /**
     * Construct an empty instance.  Default to Site option, IP type.
     */
    public Option() {
	super("", DhcptabRecord.OPTION, "");
	valueClean = false;
	vendors = new Vector();
	context = SITE;
	type = IP;
    }

    /**
     * Construct an instance from a name and definition in the syntax
     * specified by dhcptab(4).
     */
    public Option(String name, String definition) throws ValidationException {
	vendors = new Vector();
        setFlag(DhcptabRecord.OPTION);
	/*
	 * We catch exceptions generated here and do override settings.  The
	 * theory is that we create an invalid instance, but one that works
	 * as well as it can and then let a user interface eventually figure
	 * out that this isn't a valid instance and advise the user
	 * appropriately.
	 */
	try {
            setKey(name);
	} catch (ValidationException e) {
	    setKeyOverride(name);
	}
	try {
            setValue(definition);
	} catch (ValidationException e) {
	    setValueOverride(definition);
	}
    }
    
    /**
     * Set the option name as specified.
     * @param name a string of 8 characters or less
     */
    public void setKey(String name) throws ValidationException {
        if (name.length() > 8) {
            throw new ValidationException(
		ResourceStrings.getString("option_key_length"));
        }
        super.setKey(name);
    }
    
    /*
     * Set the option name even though it's not valid.
     */
    private void setKeyOverride(String name) {
	try {
            super.setKey(name);
	} catch (ValidationException e) {
	    // Ignore
	} finally {
	    validKey = false;
	}
    }

    /**
     * Get the definition as a string in the format specified by dhcptab(4)
     * @return a String containing the definition
     */
    public String getValue() {
	/* The value string stored is not clean, regenerate */
	if (!valueClean) {
	    StringBuffer b = new StringBuffer();
	    // Start with context
	    b.append(ctxts[context]);
	    // Vendor context next adds the vendors, separate by blanks
	    if (context == VENDOR) {
		boolean first = true;
		for (Enumeration e = getVendors(); e.hasMoreElements(); ) {
		    String s = (String)e.nextElement();
		    if (!first) {
			b.append(' ');
		    } else {
			first = false;
		    }
		    b.append(s);
		}
	    }
	    b.append(',');
	    // Add the code
	    b.append(code);
	    b.append(',');
	    // Add the type
	    b.append(types[type]);
	    b.append(',');
	    // Add the granularity
	    b.append(granularity);
	    b.append(',');
	    // Add the maximum
	    b.append(maximum);
	    // Save it and note as such so we can avoid doing this again
	    try {
		super.setValue(b.toString());
	    } catch (ValidationException e) {
		// This should never happen!
	    }
	    valueClean = true;   
	}
	return super.getValue();
    }
	    

    /**
     * Set the value from a string in dhcptab(4) format
     * @param definition the definition for this option
     */
    public void setValue(String definition) throws ValidationException {
	StringTokenizer st = new StringTokenizer(definition, ",");
	// See if it's basically OK
	if (st.countTokens() != 5) {
	    throw new ValidationException();
	}
	/*
	 * Now pull it apart.  Each of these will throw an exception if there
	 * is a problem, which we just let pass through.
	 */
	setContext(st.nextToken());	
	setCode(st.nextToken());
	setType(st.nextToken());
	setGranularity(st.nextToken());
	setMaximum(st.nextToken());
	
        super.setValue(definition);
	valueClean = true;
	validValue = true;
    }
    
    /**
     * Make as much sense as we can out of a definition which is not
     * valid.
     */
    private void setValueOverride(String definition) {
    	StringTokenizer st = new StringTokenizer(definition, ",");
	try {
	    setContext(st.nextToken());
	} catch (ValidationException e) {
	    // Ignore
	}
	try {
	    setCode(st.nextToken());
	} catch (ValidationException e) {
	    // Ignore
	}
	try {
	    setType(st.nextToken());
	} catch (ValidationException e) {
	    // Ignore
	}
	try {
	    setGranularity(st.nextToken());
	} catch (ValidationException e) {
	    // Ignore
	}
	try {
	    setMaximum(st.nextToken());
	} catch (ValidationException e) {
	    // Ignore
	}
	try {
	    super.setValue(definition);
	} catch (ValidationException e) {
	    // Ignore
	}
	valueClean = true;
	validValue = false;
    }

    /**
     * Return validity of this option.
     * @return true if the option is correctly defined, false if not
     */
    public boolean isValid() {
    	return (validKey && validValue);
    }

    /**
     * Get the context for this option
     * @return a byte for the option context, which is one of STANDARD, EXTEND,
     * VENDOR, or SITE.
     */
    public byte getContext() {
	return context;
    }
    
    /**
     * Set the context for this option.  Context is one of STANDARD, EXTEND,
     * VENDOR, or SITE.
     */
    public void setContext(byte c) throws ValidationException {
	if (c >= CONTEXTS) {
	    throw new ValidationException(
		ResourceStrings.getString("context_value"));
	}
	context = c;
	valueClean = false;
    }
    
    /**
     * Set the context for this option using a String representation.
     * @param s the context name, which is one of "Extend", "Site" or
     * "Vendor"
     */
    public void setContext(String s) throws ValidationException {
	byte i;
	// Find context in our defined list to get numeric representation
	for (i = 1; i < ctxts.length; ++i) {
	    if (s.startsWith(ctxts[i])) {
		break;
	    }
	}
	try {
	    setContext(i);
	} catch (ValidationException e) {
	    throw new ValidationException(ResourceStrings.getString("context"));
	}
	// Now parse the vendor portion of the context
	if (i == VENDOR) {
	    StringTokenizer st = new StringTokenizer(
		s.substring(ctxts[i].length()));
	    while (st.hasMoreTokens()) {
		addVendor(st.nextToken());
	    }
	}
    }
	
    /**
     * Enumerate the vendor list.
     * @return an Enumeration of the vendors, which will be empty for non-vendor
     * options.
     */
    public Enumeration getVendors() {
	return vendors.elements();
    }
    
    /**
     * Get the number of vendors for this option.
     * @return an int count of the vendors, zero for non-vendor options.
     */
    public int getVendorCount() {
	return vendors.size();
    }
    
    /**
     * Add a vendor to the list for this option.
     * @param v the vendor name as a String.
     */
    public void addVendor(String v) throws ValidationException {
	if (v.indexOf(',') != -1) {
	    throw new ValidationException(v);
	}
	vendors.addElement(v);
	valueClean = false;
    }
    
    /**
     * Empty the vendor list.
     */
    public void clearVendors() {
	vendors = new Vector();
	valueClean = false;
    }
    
    /**
     * Remove a vendor from the list.
     * @param index the position of the vendor to remove in the list of vendors
     */
    public void removeVendorAt(int index) {
	vendors.removeElementAt(index);
	valueClean = false;
    }
    
    /**
     * Get the vendor at a specified index in the vendor list.
     * @param index the index of the vendor to retrieve
     * @return the vendor name
     */
    public String getVendorAt(int index) {
	return (String)vendors.elementAt(index);
    }
    
    /**
     * Set the vendor name at a specified index in the list.
     * @param vendor the vendor name
     * @param index the position in the list to set.
     */
    public void setVendorAt(String vendor, int index) {
	if (index >= vendors.size()) {
	    vendors.setSize(index+1);
	}
	vendors.setElementAt(vendor, index);
	valueClean = false;
    }
    
    /**
     * Get the option code.
     * @return the code as a short.
     */
    public short getCode() {
	return code;
    }
    
    /**
     * Set the option code.  Validates against legal range for the context.
     * @param c the code to use
     */
    public void setCode(short c) throws ValidationException {
	switch (context) {
	case STANDARD:
	    if (((c < STANDARD_BEGIN) || (c > STANDARD_END)) 
		    && ((c < SPECIAL_BEGIN) || (c > SPECIAL_END))) {
		throw new ValidationException(
		    ResourceStrings.getString("standard_code"));
	    }
	    break;
	case EXTEND:
	    if ((c < EXTEND_BEGIN) || (c > EXTEND_END)) {
		throw new ValidationException(
		    ResourceStrings.getString("extend_code"));
	    }
	    break;
	case SITE:
	    if ((c < SITE_BEGIN) || (c > SITE_END)) {
		throw new ValidationException(
		    ResourceStrings.getString("site_code"));
	    }
	    break;
	case VENDOR:
	    if ((c < VENDOR_BEGIN) || (c > VENDOR_END)) {
		throw new ValidationException(
		    ResourceStrings.getString("vendor_code"));
	    }
	    break;
	default:
	    throw new ValidationException(ResourceStrings.getString("context"));
	}
	code = c;
	valueClean = false;
    }
    
    /**
     * Set the code from a string.
     * @param s the code as a string.
     */
    public void setCode(String s) throws ValidationException {
	try {
	    setCode(Short.parseShort(s));
	} catch (NumberFormatException e) {
	    throw new ValidationException(s);
	}
    }
    
    /**
     * Get the type.
     * @return a byte value for the type, one of ASCII, BOOLEAN, IP, NUMBER, or
     * OCTET
     */
    public byte getType() {
	return type;
    }
    
    /**
     * Set the type.
     * @param t the type code, which must be one of ASCII, BOOLEAN, IP, NUMBER,
     * or OCTET.
     */
    public void setType(byte t) throws ValidationException {
	if (t >= TYPES) {
	    throw(new ValidationException(
		ResourceStrings.getString("type_value")));
	}
	type = t;
	valueClean = false;
    }
    
    /**
     * Set the type as a String.
     * @param s the type code as a String.  Must be "ASCII", "BOOLEAN", "IP",
     * "NUMBER", or "OPTION".
     */
    public void setType(String s) throws ValidationException {
	byte i;
	for (i = 0; i < TYPES; ++i) {
	    if (s.equals(types[i])) {
		break;
	    }
	}
	try {
	    setType(i);
	} catch (ValidationException e) {
	    throw new ValidationException(ResourceStrings.getString("type"));
	}
    }

    /**
     * Get the granularity.  See dhcptab(4) for an explanation of granularity
     * interpretations.
     * @return the granularity as an int
     */
    public int getGranularity() {
	return granularity;
    }
    
    /**
     * Set the granularity.  See dhcptab(4) for an explanation of granularity
     * interpretations.
     * @param g the granularity as an int.
     */
    public void setGranularity(int g) {
	granularity = g;
	valueClean = false;
    }
    
    /**
     * Set the granularity using a String representation.
     * @param s the granularity as a String.
     */
    public void setGranularity(String s) throws ValidationException {
	try {
	    setGranularity(Integer.parseInt(s));
	} catch (NumberFormatException e) {
	    throw new ValidationException(s);
	}
    }
    
    /**
     * Get the maximum.  See dhcptab(4) for an explanation of maximum.
     * @return the maximum as an int.
     */
    public int getMaximum() {
	return maximum;
    }
    
    /**
     * Set the maximum.  See dhcptab(4) for an explanation of maximum.
     * @param m the maximum as an int.
     */
    public void setMaximum(int m) {
	maximum = m;
	valueClean = false;
    }
    
    /**
     * Set the maximum from a String.
     * @param s the maximum as a String.
     */
    public void setMaximum(String s) throws ValidationException {
	try {
	    setMaximum(Integer.parseInt(s));
	} catch (NumberFormatException e) {
	    throw new ValidationException(s);
	}
    }
    
    // Make a copy of this option
    public Object clone() {
	Option o = new Option();
	try {
	    o.setKey(getKey());
	    o.setContext(getContext());
	    o.setCode(getCode());
	    o.vendors = new Vector();
	    for (Enumeration en = vendors.elements(); en.hasMoreElements(); ) {
	        String s = (String)en.nextElement();
		o.vendors.addElement(new String(s));
	    }
	    o.setType(getType());
	    o.setGranularity(getGranularity());
	    o.setMaximum(getMaximum());
	} catch (ValidationException e) {
	    // Can't happen if everything worked right when creating this one
	} finally {
	    return o;
	}
    }
    
    public String toString() {
	return (getKey() + " s " + getValue());
    }
}
