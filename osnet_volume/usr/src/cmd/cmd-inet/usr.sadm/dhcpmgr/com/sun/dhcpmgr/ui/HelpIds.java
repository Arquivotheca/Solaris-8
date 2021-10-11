/*
 * @(#)HelpIds.java	1.3	99/05/10 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.ui;

import java.util.*;

public class HelpIds {
    private ResourceBundle bundle;
    
    public HelpIds(String bundleName) throws MissingResourceException {
	bundle = ResourceBundle.getBundle(bundleName);
    }
        
    public String getFilePath(String key) {
	try {
	    /*
	     * The original version of this code was:
	     * if (bundle.getLocale().toString().length() == 0) {
	     * bug 4177489 causes that not to work correctly, so for the moment
	     * we *require* that each key in a locale contain a relative
	     * path, otherwise it is assumed we're in the default locale and
	     * proceed with the default location.
	     */
	    String s = bundle.getString(key);
	    if (s.indexOf('/') == -1) {
	    	// Not localized, use the default location
		return "/usr/sadm/admin/dhcpmgr/help/" + s;
	    } else {
	    	return "/usr/share/lib/locale/com/sun/dhcpmgr/client/help/" + s;
	    }
	} catch (Throwable e) {
	    e.printStackTrace();
	    return "";
	}
    }
}
