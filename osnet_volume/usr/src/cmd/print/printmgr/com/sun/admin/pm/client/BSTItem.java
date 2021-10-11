/*
 *
 * ident	"@(#)BSTItem.java	1.4	99/03/29 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * BSTItem.java
 * Simple binary search tree implementation for help articles
 */

package com.sun.admin.pm.client;

import java.lang.*;
import com.sun.admin.pm.server.*;

public class BSTItem extends Object {
    public String key;
    public Object data;
    public int handle = UNINITIALIZED;
  
    static int serial = 0;
    static final int UNINITIALIZED = -1;

    public BSTItem(String newKey) {
        this(newKey, null);
    }

    public BSTItem(String newKey, Object obj) {
        key = newKey.toLowerCase();
        data = obj;
        handle = serial++;
    }

    public String toString() {
        return new String("Item " + key + " (" + handle + ")");
    }

    public int compare(BSTItem otherItem, boolean exact) {
    
        return compare(otherItem.key, exact);
    }


    public int compare(BSTItem otherItem) {
        return compare(otherItem, true);
    }

    public int compare(String otherKey) {
        return compare(otherKey, true);
    }


    public int compare(String otherKey, boolean exact) {

        /*
         * System.out.println(this.toString() + " comparing " +
         * (exact ? "exact" : "partial") + " to " + otherKey);
         */

        int rv = 0;

        if (otherKey != null && otherKey != "")
            rv = exact ?
                key.compareTo(otherKey) :
                compareSub(otherKey.toLowerCase());

        /*
	 *  System.out.println(
         *       "Compare: " + key + " to " + otherKey + " -> " + rv);
	 */

        return rv;
    }


    public int compareSub(String s) {
        Debug.info("HELP:  compareSub: " + key + " to " + s);
    
        int rv = 0;
        try {
            rv = key.substring(0, s.length()).compareTo(s);
        } catch (Exception x) {
            Debug.info("HELP:  compareSub caught: " + x);
            rv = -1;
        }
        return rv;
    }
}














