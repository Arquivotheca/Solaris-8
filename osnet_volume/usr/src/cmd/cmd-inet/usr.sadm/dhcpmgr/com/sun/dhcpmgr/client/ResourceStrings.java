/*
 * @(#)ResourceStrings.java	1.1	99/03/22 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.client;

import java.util.*;

public class ResourceStrings {
    private static ResourceBundle bundle = null;
    
    public static String getString(String key) {
	if (bundle == null) {
	    bundle = ResourceBundle.getBundle(
		"com.sun.dhcpmgr.client.ResourceBundle", Locale.getDefault());
	}
	return bundle.getString(key);
    }
}
