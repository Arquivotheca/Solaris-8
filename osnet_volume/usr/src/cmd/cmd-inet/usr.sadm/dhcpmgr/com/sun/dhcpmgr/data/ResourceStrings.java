/*
 * @(#)ResourceStrings.java	1.1	99/03/22 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.data;

import java.util.*;

/**
 * This class encapsulates the lookup of resources in the resource bundle
 * for this package.  The resource bundle is cached for maximum performance.
 * @see ResourceBundle
 */
public class ResourceStrings {
    private static ResourceBundle bundle = null;
    
    /**
     * Retrieve the value associated with a particular resource key.
     * @param key the resource we need
     * @return a String containing the value.
     */
    public static String getString(String key) {
	if (bundle == null) {
	    bundle = ResourceBundle.getBundle(
		"com.sun.dhcpmgr.data.ResourceBundle", Locale.getDefault());
	}
	return bundle.getString(key);
    }
}
