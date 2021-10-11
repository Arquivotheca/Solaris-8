/*
 * @(#)AsciiOptionValue.java	1.2	99/04/28 SMI
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.data;

import java.io.Serializable;
import java.util.Vector;
import java.util.Enumeration;

public class AsciiOptionValue implements OptionValue, Serializable {
    private String name;
    private String value;
    private boolean valid;
    
    protected AsciiOptionValue(String name) {
	this.name = name;
	value = null;
	valid = false;
    }
    
    public String getName() {
	return name;
    }
    
    public String getValue() {
    	// Before we return the value, we go through and escape special chars.
	StringBuffer retValue = new StringBuffer();
	char [] c = value.toCharArray();
	for (int i = 0; i < c.length; ++i) {
	    if (c[i] == '\\' || c[i] == '"') {
	        retValue.append('\\');
	    }
	    retValue.append(c[i]);
	}
	return retValue.toString();
    }
    
    public void setValue(Object value) throws ValidationException {
	// Find option in option definition table in order to validate the data
	Option option = OptionsTable.getTable().get(name);
	if (option == null) {
	    throw new ValidationException();
	}
	if (value instanceof String) {
	    String newValue = (String)value;
	    // Either quoted, or not, but must balance
	    if (newValue.startsWith("\"") ^ newValue.endsWith("\"")) { 
		throw new ValidationException();
	    }
	    if (newValue.startsWith("\"")) {
		newValue = newValue.substring(1, newValue.length() - 1);
	    }
	    if (newValue.length() == 0) {
		// Empty strings are not acceptable
		throw new ValidationException();
	    }
	    // Check that the resulting length is OK
	    if ((option.getMaximum() != 0)
		    && (newValue.length() > option.getMaximum())) {
		throw new ValidationException();
	    }
	    this.value = newValue;
	    valid = true;
	} else if (value instanceof Vector) {
	    /*
	     * We generate the value by creating a blank-separated list of
	     * tokens; each token is the product of a toString() on the
	     * vector's elements.
	     */
	    StringBuffer b = new StringBuffer();
	    Enumeration en = ((Vector)value).elements();
	    while (en.hasMoreElements()) {
		if (b.length() != 0) {
		    b.append(' ');
		}
		b.append(en.nextElement().toString());
	    }
	    setValue(b.toString());
	} else {
	    // Anything else should just tell us what it looks like as a string.
	    setValue(value.toString());
	}
    }
    
    public String toString() {
	return (getName() + "=\"" + getValue() + "\"");
    }
    
    public boolean isValid() {
	return valid;
    }
    
    public Object clone() {
	AsciiOptionValue v = new AsciiOptionValue(name);
	v.value = value;
	v.valid = valid;
	return v;
    }
}
