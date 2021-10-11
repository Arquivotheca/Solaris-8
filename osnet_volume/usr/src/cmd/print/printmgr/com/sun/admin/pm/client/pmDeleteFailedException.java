/*
 *
 * ident	"@(#)pmDeleteFailedException.java	1.3	99/03/29 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmDeleteFailedException.java
 * 
 */

package com.sun.admin.pm.client;

import java.lang.*;

class pmDeleteFailedException extends pmGuiException {
    public pmDeleteFailedException(String s) {
        super(s);
    }
    public pmDeleteFailedException() {
        super();
    }
}


