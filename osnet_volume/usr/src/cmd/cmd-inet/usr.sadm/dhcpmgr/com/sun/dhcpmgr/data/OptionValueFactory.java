/*
 * @(#)OptionValueFactory.java	1.2	99/03/31 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.data;

import java.util.Vector;

/**
 * This class provides the functionality to construct an option value of the
 * correct type when only the tag we associate with the option value is known.
 */
public class OptionValueFactory {
    private static OptionsTable optionsTable = OptionsTable.getTable();

    /**
     * Construct an option value given the name, and initialize it to the
     * provided value.
     * @param name the name of the option
     * @param value the initial value for the option
     * @return an OptionValue of the correct type for this option.  If the name
     * or value supplied is invalid in some way, an instance of
     * BogusOptionValue is returned and the caller should take appropriate
     * action.
     */
    public static OptionValue newOptionValue(String name, Object value) {
	OptionValue v;
	try {
	    v = newOptionValue(name);
	    v.setValue(value);
	} catch (ValidationException e) {
	    // Not a valid value; put it in the bogus value placeholder
	    v = new BogusOptionValue(name, value);
	}
	return v;
    }
    
    /**
     * Construct an empty option value given the name
     * @param name the name of the option
     * @return an OptionValue of the correct type for this option.
     */
    public static OptionValue newOptionValue(String name) {
	if (name.length() == 0) {
	    // Empty name is not acceptable
	    return new BogusOptionValue(name);
	}
	Option opt = optionsTable.get(name);
	if (opt == null) {
	    // Include is not in the options table
	    if (name.equals("Include")) {
		return new IncludeOptionValue();
	    } else {
	    	/*
		 * Bogus option name; create a bogus value that callers
		 * can pick up later.
		 */
		 return new BogusOptionValue(name);
	    }
	}
	switch (opt.getType()) {
	case Option.ASCII:
	    return new AsciiOptionValue(name);
	case Option.BOOLEAN:
	    return new BooleanOptionValue(name);
	case Option.IP:
	    return new IPOptionValue(name);
	case Option.NUMBER:
	    return new NumberOptionValue(name);
	case Option.OCTET:
	    return new OctetOptionValue(name);
	default:
	    // This should never happen
	    return new BogusOptionValue(name);
	}
    }
}
