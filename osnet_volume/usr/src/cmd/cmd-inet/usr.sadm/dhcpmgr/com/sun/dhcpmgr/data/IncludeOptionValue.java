/*
 * @(#)IncludeOptionValue.java	1.1	99/03/22 SMI
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.data;

import java.io.Serializable;
import java.util.Vector;

public class IncludeOptionValue implements OptionValue, Serializable {
    private String value;
    private boolean valid;
    
    protected IncludeOptionValue() {
	value = "";
	valid = false;
    }
    
    public void setValue(Object value) throws ValidationException {
	if (value instanceof String) {
	    if (((String)value).length() == 0) {
		// Empty values are not acceptable
		throw new ValidationException();
	    }
	    this.value = (String)value;
	    valid = true;
	} else {
	    setValue(value.toString());
	}
    }
    
    public String getName() {
	return "Include";
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
	IncludeOptionValue v = new IncludeOptionValue();
	v.value = value;
	v.valid = valid;
	return v;
    }
}
