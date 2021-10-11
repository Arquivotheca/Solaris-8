/*
 * @(#)ResourceStrings.java	1.1	99/03/22 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.ui;

import java.util.*;

/**
 * This class provides a simpler, cached lookup of resources for members of
 * this package.
 */
public class ResourceStrings {
    private static ResourceBundle bundle = null;
    
    /**
     * Retrieve the resource named by the provided key
     * @param key the resource to look up
     * @return the value of the resource
     */
    public static String getString(String key) {
	if (bundle == null) {
	    bundle = ResourceBundle.getBundle(
		"com.sun.dhcpmgr.ui.ResourceBundle", Locale.getDefault());
	}
	return bundle.getString(key);
    }
}
