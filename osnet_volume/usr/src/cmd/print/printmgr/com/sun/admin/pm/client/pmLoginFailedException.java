/*
 *
 * ident	"@(#)pmLoginFailedException.java	1.3	99/03/29 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmLoginFailedException.java
 * 
 */

package com.sun.admin.pm.client;

import java.lang.*;

class pmLoginFailedException extends pmGuiException {
    public pmLoginFailedException(String s) {
        super(s);
    }
    public pmLoginFailedException() {
        super();
    }
}


