/*
 *
 * ident	"@(#)pmUserCancelledException.java	1.4	99/03/29 SMI"
 * 
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmUserCancelledException .java
 * 
 */

package com.sun.admin.pm.client;

import java.lang.*;

class pmUserCancelledException extends pmGuiException {
    public pmUserCancelledException(String s) {
        super(s);
    }
    public pmUserCancelledException() {
        super();
    }
}


