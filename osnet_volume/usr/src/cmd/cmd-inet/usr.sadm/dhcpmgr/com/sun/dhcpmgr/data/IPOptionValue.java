/*
 * @(#)IPOptionValue.java	1.2	99/05/07 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.data;

import java.io.Serializable;
import java.net.*;
import java.util.*;

public class IPOptionValue implements OptionValue, Serializable {
    private String name;
    private Vector addrs;
    private boolean valid;
    
    protected IPOptionValue(String name) {
	this.name = name;
	addrs = null;
	valid = false;
    }
    
    public void setValue(Object value) throws ValidationException {
	// Find option in option definition table in order to validate the data
	Option option = OptionsTable.getTable().get(name);
	if (option == null) {
	    throw new ValidationException();
	}
	Vector newAddrs = new Vector();
	if (value instanceof String) {	
	    if (((String)value).length() == 0) {
		// Empty strings aren't acceptable
		throw new ValidationException();
	    }    
	    /*
	     * Break string apart at whitespace and use it to construct
	     * a vector of IPAddresses
	     */
	    StringTokenizer st = new StringTokenizer((String)value, " ");
	    while (st.hasMoreTokens()) {
		newAddrs.addElement(new IPAddress(st.nextToken()));
	    }
	} else if (value instanceof InetAddress) {
	    newAddrs.addElement(value);
	} else if (value instanceof IPAddress) {
	    newAddrs.addElement(value);
	} else if (!(value instanceof Vector)) {
	    // Can't handle anything else but a vector of addresses
	    throw new ValidationException();
	} else {
	    // Make sure vector only contains InetAddresses or IPAddresses
	    newAddrs = (Vector)value;
	    for (Enumeration en = newAddrs.elements(); en.hasMoreElements(); ) {
		Object o = en.nextElement();
		if (!(o instanceof InetAddress) && !(o instanceof IPAddress)) {
		    throw new ValidationException();
		}
	    }
	}
	if ((newAddrs.size() % option.getGranularity()) != 0) {
	    throw new ValidationException();
	}
	if ((option.getMaximum() != 0) 
		&& (newAddrs.size() > option.getMaximum())) {
	    throw new ValidationException();
	}
	
	addrs = newAddrs;
	valid = true;
    }
    
    public String getName() {
	return name;
    }
    
    public String getValue() {
	if (addrs == null || addrs.size() == 0) {
	    return "";
	}
	StringBuffer buf = new StringBuffer();
	for (Enumeration en = addrs.elements(); en.hasMoreElements(); ) {
	    Object o = en.nextElement();
	    if (buf.length() != 0) {
		buf.append(' ');
	    }
	    if (o instanceof IPAddress) {
		buf.append(((IPAddress)o).getHostAddress());
	    } else {
		buf.append(((InetAddress)o).getHostAddress());
	    }
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
	IPOptionValue v = new IPOptionValue(name);
	if (addrs != null) {
		v.addrs = (Vector)addrs.clone();
	}
	v.valid = valid;
	return v;
    }
}
