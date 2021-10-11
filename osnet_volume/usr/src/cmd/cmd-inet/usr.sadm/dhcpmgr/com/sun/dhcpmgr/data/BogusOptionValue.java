/*
 * @(#)BogusOptionValue.java	1.2	99/05/05 SMI
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

package com.sun.dhcpmgr.data;

import java.io.Serializable;
import java.util.Vector;
import java.util.Enumeration;

/**
 * This class provides a way for us to handle errors in the dhcptab which
 * may have been introduced through the command line or direct editing of
 * the table.  The idea is for the OptionValueFactory to trap bad option
 * names or values and store them in an instance of this class so that the
 * user can then be told about the error and allowed to fix it.
 */
public class BogusOptionValue implements OptionValue, Serializable {
    private String name;
    private String value;
    
    protected BogusOptionValue(String name) {
	this.name = name;
	value = null;
    }
    
    protected BogusOptionValue(String name, Object value) {
    	this.name = name;
	setValue(value);
    }

    public String getName() {
	return name;
    }
    
    public String getValue() {
	return value;
    }
    
    public void setValue(Object value) {
	if (value instanceof Vector) {
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
	} else if (value instanceof String) {
	    this.value = (String)value;
	} else {
	    // Anything else should just tell us what it looks like as a string.
	    setValue(value.toString());
	}
    }
    
    public String toString() {
	return (getName() + "=\"" + getValue() + "\"");
    }
    
    public boolean isValid() {
	// This kind of option is never valid
	return false;
    }
    
    public Object clone() {
	return new BogusOptionValue(name, value);
    }
}
