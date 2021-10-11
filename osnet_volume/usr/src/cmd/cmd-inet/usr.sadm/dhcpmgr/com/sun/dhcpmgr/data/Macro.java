/*
 * @(#)Macro.java	1.2 99/04/28 SMI
 *
 * Copyright (c) 1998-1999 Sun Microsystems, Inc.
 * All rights reserved.
 */

package com.sun.dhcpmgr.data;

import java.io.Serializable;
import java.util.*;

/**
 * Macro is a simple data class which encapsulates a macro record in
 * the dhcptab.  See dhcptab(4) for the gory details on macros.
 *
 * @version     1.2, 04/28/99
 * @author      Dave Miner
 * @see DhcptabRecord
 * @see Option
 */
public class Macro extends DhcptabRecord implements Serializable, Cloneable {
    private boolean valueClean = false;
    private Vector options;

    public Macro() {
        super("", DhcptabRecord.MACRO, "");
	options = new Vector();
    }
    
    public Macro(String name) throws ValidationException {
	this();
	setKey(name);
    }

    public Macro(String name, String expansion) throws ValidationException {
	options = new Vector();
        setKey(name);
        setFlag(DhcptabRecord.MACRO);
        setValue(expansion);
    }
    
    public void setKey(String name) throws ValidationException {
        if (name.length() > 64) {
            throw new ValidationException();
        }
        super.setKey(name);
    }

    public void setValue(String expansion) throws ValidationException {
	StringBuffer symbol = new StringBuffer();
	StringBuffer value = new StringBuffer();
	boolean inQuote = false;
	boolean inEscape = false;
	char c;
	// State list for parsing machine
	int START = 0;
	int NAME = 1;
	int VALUE = 2;
	int state = START;

	options.removeAllElements(); // Empty hashtable of options
	for (int i = 0; i < expansion.length(); ++i) {
	    c = expansion.charAt(i);
	    if (state == START) {
		// Start of expansion
		if (c != ':') {
		    throw new ValidationException();
		}
		state = NAME;
	    } else if (state == NAME) {
		// Name of symbol
		if (c == '=') {
		    state = VALUE;
		} else if (c == ':') {
		    storeOption(symbol.toString(), value.toString());
		    symbol.setLength(0);
		    value.setLength(0);
		    state = NAME;			
		} else {
		    symbol.append(c);
		}
	    } else if (state == VALUE) {
		// Value of symbol
		if (inEscape) {
		    value.append(c);
		    inEscape = false;
		} else if (c == '\\') {
		    inEscape = true;
		} else if (c == '"') {
		    inQuote = !inQuote;
		    value.append(c);
		} else if (inQuote) {
		    value.append(c);
		} else if (c == ':') {
		    storeOption(symbol.toString(), value.toString());
		    symbol.setLength(0);
		    value.setLength(0);
		    state = NAME;
		} else {
		    value.append(c);
		}
	    }
	}
	if (state != NAME) {
	    throw new ValidationException("Syntax error in " + getKey());
	}
        super.setValue(expansion);
	valueClean = true;
    }

    public void storeOption(String option, Object value)
	    throws ValidationException {
	options.addElement(OptionValueFactory.newOptionValue(option, value));
    }
    
    // Useful for creating options when standard code values are known.
    public void storeOption(int code, Object value) throws ValidationException {
	options.addElement(
	    OptionValueFactory.newOptionValue(StandardOptions.nameForCode(code),
	    value));
    }
    
    public String getValue() {
	boolean first;
	if (!valueClean) {
	    // Construct a new value
	    StringBuffer buf = new StringBuffer();
	    for (Enumeration e = options.elements(); e.hasMoreElements(); ) {
		OptionValue v = (OptionValue)e.nextElement();
		if (v == null) {
		    continue;	// Ignore an empty position
		}
		buf.append(':');
		buf.append(v.toString());
    	    }
	    buf.append(':');
	    try {
	        super.setValue(buf.toString());
	    } catch (ValidationException ex) {
		// Shouldn't happen; ignore it
	    }
	    valueClean = true;
	}
        return super.getValue();
    }
    
    public Enumeration elements() {
	return options.elements();
    }
    
    public OptionValue [] getOptions() {
	OptionValue [] optArray = new OptionValue[options.size()];
	options.copyInto(optArray);
	return optArray;
    }
    
    public OptionValue getOption(String name) {
	for (Enumeration en = options.elements(); en.hasMoreElements(); ) {
	    OptionValue v = (OptionValue)en.nextElement();
	    if (name.equals(v.getName())) {
		return v;
	    }
	}
	return null;
    }
    
    public OptionValue getOptionAt(int index) {
	return (OptionValue)options.elementAt(index);
    }
    
    public void setOptionAt(OptionValue v, int index) {
	if (index >= options.size()) {
	    options.setSize(index + 1); // Grow vector if necessary
	}
	options.setElementAt(v, index);
    }	
    
    public int optionCount() {
	return options.size();
    }
    
    public void deleteOptionAt(int index) {
	if (index >= options.size()) {
	    return;
	}
	options.removeElementAt(index);
    }
    
    public void insertOptionAt(OptionValue v, int index) {
	options.insertElementAt(v, index);
    }
    
    // Make a copy of this macro
    public Object clone() {
	Macro m = new Macro();
	m.key = key;
	m.options = new Vector();
	for (Enumeration en = options.elements(); en.hasMoreElements(); ) {
	    OptionValue v = (OptionValue)en.nextElement();
	    m.options.addElement((OptionValue)v.clone());
	}
	return m;
    }
    
    public String toString() {
	return (getKey() + " m " + getValue());
    }
    
    // Verify that the options contained in this macro are all valid
    public void validate() throws ValidationException {
	for (Enumeration en = options.elements(); en.hasMoreElements(); ) {
	    OptionValue v = (OptionValue)en.nextElement();
	    if (!v.isValid()) {
		throw new ValidationException(v.getName());
	    }
	}
    }
}
