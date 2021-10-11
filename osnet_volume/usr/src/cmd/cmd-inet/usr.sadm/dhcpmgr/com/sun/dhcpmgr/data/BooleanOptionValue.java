/*
 * @(#)BooleanOptionValue.java	1.2	99/05/05 SMI
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.data;

import java.io.Serializable;
import java.util.Vector;

public class BooleanOptionValue implements OptionValue, Serializable {
    private String name;
    
    protected BooleanOptionValue(String name) {
	this.name = name;
    }
    
    public String getName() {
	return name;
    }
    
    public String getValue() {
	return "";
    }
    
    public void setValue(Object value) throws ValidationException {
	// Booleans must have an empty value
	if (value != null && value.toString().length() != 0) {
	    throw new ValidationException();
	}
    }
    
    public String toString() {
	return getName();
    }
    
    public boolean isValid() {
	return true;
    }
    
    public Object clone() {
	return new BooleanOptionValue(name);
    }
}
