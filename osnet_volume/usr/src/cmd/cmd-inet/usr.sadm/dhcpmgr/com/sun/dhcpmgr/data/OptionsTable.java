/*
 * @(#)OptionsTable.java	1.3	99/07/30 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.data;

import java.util.*;
import java.io.Serializable;

/**
 * This class provides a global table of all the options currently known.
 * It is implemented as a singleton as there should be no need for more
 * than a single instance of this table.  It includes both the standard
 * options and any vendor or site options defined in the current environment.
 */
public class OptionsTable implements Serializable {
    private Hashtable options;
    private static OptionsTable table = null;
    
    protected OptionsTable() {
	// Get the standard options we know about
	StandardOptions stdopts = new StandardOptions();

	// Initialize hash table with extra size we will probably need.
	options = new Hashtable(stdopts.size() + 20);

	// Add the standard options to the table
	add(stdopts.getAllOptions());
    }
    
    /**
     * Add an array of options to the table.
     * @param opts An array of Options
     */
    public void add(Option [] opts) {
	for (int i = 0; i < opts.length; ++i) {
	    add(opts[i]);
	}
    }
    
    /**
     * Add a single option to the table.
     * @param o The option to add.
     */
    public void add(Option o) {
	// Don't add unless it is a valid option.
	if (o.isValid()) {
	    options.put(o.getKey(), o);
	}
    }
    
    /**
     * Retrieve an option from the table by name
     * @param opt the name of the option to retrieve
     * @return the option found, or null if the option is not in the table
     */
    public Option get(String opt) {
	return (Option)options.get(opt);
    }
    
    /**
     * Enumerate the options in this table for those that might need to walk it.
     * @return an Enumeration of the options
     */
    public Enumeration elements() {
	return options.elements();
    }
    
    /**
     * Return the global table, create it if not already in existence.
     * @return the current options table
     */
    public static OptionsTable getTable() {
	if (table == null) {
	    table = new OptionsTable();
	}
	return table;
    }
}
