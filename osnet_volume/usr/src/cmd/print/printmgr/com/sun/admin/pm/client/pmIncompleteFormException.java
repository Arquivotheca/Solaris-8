/*
 *
 * ident	"@(#)pmIncompleteFormException.java	1.3	99/03/29 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmIncompleteFormException.java
 * 
 */

package com.sun.admin.pm.client;

import java.lang.*;

class pmIncompleteFormException extends pmGuiException {
    public pmIncompleteFormException(String s) {
        super(s);
    }
    public pmIncompleteFormException() {
        super();
    }
}


