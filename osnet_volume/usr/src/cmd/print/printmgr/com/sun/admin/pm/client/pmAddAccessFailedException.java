/*
 *
 * ident	"@(#)pmAddAccessFailedException.java	1.3	99/03/29 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmAddAccessFailedException.java
 * 
 */

package com.sun.admin.pm.client;

import java.lang.*;

class pmAddAccessFailedException extends pmGuiException {
    public pmAddAccessFailedException(String s) {
        super(s);
    }
    public pmAddAccessFailedException() {
        super();
    }
}


