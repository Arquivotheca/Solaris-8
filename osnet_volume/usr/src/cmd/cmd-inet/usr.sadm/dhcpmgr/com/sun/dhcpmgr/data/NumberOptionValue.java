/*
 * @(#)NumberOptionValue.java	1.4	99/05/07 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.data;

import java.io.Serializable;
import java.util.*;

/**
 * This class provides a way to retain the radix specified by the user
 * when entering the data.  Currently only support 10 and 16 for radix.
 */
class NumberValue {
    private long value;
    private int radix;

    public NumberValue(Number value, int radix) {
	// Convert all numbers to long
    	this.value = value.longValue();
	this.radix = radix;
    }

    public String toString() {
	// Handle hex specially
	if (radix == 16) {
	    return "0x" + Long.toHexString(value);
	} else {
	    return Long.toString(value);
	}
    }
}

public class NumberOptionValue implements OptionValue, Serializable {
    private String name;
    private Vector nums;
    private boolean valid;
    private int radix;

    protected NumberOptionValue(String name) {
	this.name = name;
	nums = null;
	valid = false;
    }
    
    public void setValue(Object value) throws ValidationException {
	// Find option in option definition table in order to validate the data
	Option option = OptionsTable.getTable().get(name);
	if (option == null) {
	    throw new ValidationException();
	}
	Vector newNums = new Vector();
	if (value instanceof String) {
	    if (((String)value).length() == 0) {
		// Empty strings are not acceptable
		throw new ValidationException();
	    }
	    // Parse each token into an object of the correct numeric type
	    StringTokenizer st = new StringTokenizer((String)value, " ");
	    while (st.hasMoreTokens()) {
		int radix = 10;
		String s = st.nextToken();
		if (s.startsWith("0x") || s.startsWith("0X")) {
		    radix = 16;
		    s = s.substring(2);
		} else if (s.startsWith("0")) {
		    radix = 8;
		    s = s.substring(1);
		}
		Number n;
		try {
		    switch (option.getGranularity()) {
		    case 1:
			n = Byte.valueOf(s, radix);
			break;
		    case 2:
			n = Short.valueOf(s, radix);
			break;
		    case 4:
			n = Integer.valueOf(s, radix);
			break;
		    case 8:
			n = Long.valueOf(s, radix);
			break;
		    default:
			throw new ValidationException();
		    }
		    newNums.addElement(new NumberValue(n, radix));
		} catch (NumberFormatException e) {
		    throw new ValidationException();
		}
	    }
	} else if (value instanceof Number) {
	    newNums.addElement(new NumberValue((Number)value, 10));
	} else if (!(value instanceof Vector)) {
	    throw new ValidationException();
	} else {
	    // Caller supplied a vector; make sure each value is a number
	    Enumeration en = ((Vector)value).elements();
	    while (en.hasMoreElements()) {
	        Object o = en.nextElement();
		if (!(o instanceof Number)) {
		    throw new ValidationException();
		} else {
		    newNums.addElement(new NumberValue((Number)o, 10));
	        }
	    }
	}
	
	// We now have a vector of numbers; check count against expected
	if ((option.getMaximum() != 0)
		&& (newNums.size() > option.getMaximum())) {
	    throw new ValidationException();
	}

	nums = newNums;
	valid = true;
    }
    
    public String getName() {
	return name;
    }
    
    public String getValue() {
	if (nums == null || nums.size() == 0) {
	    return "";
	}
	StringBuffer buf = new StringBuffer();
	for (Enumeration en = nums.elements(); en.hasMoreElements(); ) {
	    if (buf.length() != 0) {
		buf.append(' ');
	    }
	    buf.append(en.nextElement().toString());
	}
	return buf.toString();
    }
    
    public String toString() {
	return (getName() + "=" + getValue());
    }

    public boolean isValid() {
	return valid;
    }
    
    public Object clone() {
	NumberOptionValue v = new NumberOptionValue(name);
	if (nums != null) {
	    v.nums = (Vector)nums.clone();
	}
	v.valid = valid;
	return v;
    }
}
