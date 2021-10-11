/*
 *
 * ident	"@(#)pmHelpContent.java	1.2	99/03/29 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmHelpContent.java
 * Wrapper for content of help articles
 */

package com.sun.admin.pm.client;

class pmHelpContent extends Object {
    private String text;
  
    public pmHelpContent(String content) {
		text = content;
	}

    public pmHelpContent(pmHelpContent other) {
		text = new String(other.text);
	}

    public String toString() {
		return text;
	}

    public String getText() {
		return text;
	}
}
