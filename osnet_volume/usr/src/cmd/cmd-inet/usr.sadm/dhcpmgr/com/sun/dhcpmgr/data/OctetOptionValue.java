/*
 * @(#)OctetOptionValue.java	1.2	99/05/05 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.data;

import java.io.Serializable;
import java.util.Vector;
import java.util.Enumeration;

public class OctetOptionValue implements OptionValue, Serializable {
    private String name;
    private String value;
    private boolean valid;
    
    protected OctetOptionValue(String name) {
	this.name = name;
	value = "";
	valid = false;
    }
    
    public void setValue(Object value) throws ValidationException {
	// Find option in option definition table in order to validate the data
	Option option = OptionsTable.getTable().get(name);
	if (option == null) {
	    throw new ValidationException();
	}
	if (value instanceof String) {
	    if (((String)value).length() == 0) {
		// Empty values are not acceptable
		throw new ValidationException();
	    }
	    // Just make a copy of the reference
	    this.value = (String)value;
	    valid = true;
	} else if (value instanceof Vector) {
	    /*
	     * Generate the value by concatenating toString()'s on the
	     * vector's elements
	     */
	    StringBuffer b = new StringBuffer();
	    Enumeration en = ((Vector)value).elements();
	    while (en.hasMoreElements()) {
		b.append(en.nextElement().toString());
	    }
	    setValue(b.toString());
	} else {
	    // Convert anything else to a string
	    setValue(value.toString());
	}
    }

    public String getName() {
	return name;
    }
    
    public String getValue() {
	return value;
    }
    
    public String toString() {
	return (getName() + "=" + getValue());
    }

    public boolean isValid() {
	return valid;
    }
    
    public Object clone() {
	OctetOptionValue v = new OctetOptionValue(name);
	v.value = value;
	v.valid = valid;
	return v;
    }
}
