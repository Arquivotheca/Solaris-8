/*
 * ident	"@(#)Assert.java	1.2	99/05/10 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 */

//
// Assert.java : Handles assertions in a central fashion.
//
//  Author:           Erik Guttman
//
//

package com.sun.slp;

import java.util.*;
import java.text.*;

/**
 * The Assert class is used to test assertions and end the program
 * execution if the assertion fails.
 *
 * @author  Erik Guttman
 * @version @(#)Assert.java	1.2 05/10/99
 */

class Assert {
    static void assert(boolean bool, String msgTag, Object[] params) {
	if (bool == false) {
	    SLPConfig conf = SLPConfig.getSLPConfig();
	    printMessageAndDie(conf, msgTag, params);
	}
    }

    // Print message and die. Used within SLPConfig during initialization.
    static void 
	printMessageAndDie(SLPConfig conf, String msgTag, Object[] params) {
	ResourceBundle msgs = conf.getMessageBundle(conf.getLocale());
	String failed = msgs.getString("assert_failed");
	String msg = conf.formatMessage(msgTag, params);
	System.err.println(failed+msg);
	(new Exception()).printStackTrace();  // tells where we are at...
	System.exit(-1);
    }
  
    // Assert that a parameter is nonnull.
    // Throw IllegalArgumentException if so.

    static void nonNullParameter(Object obj, String param) {
	if (obj == null) {
	    SLPConfig conf = SLPConfig.getSLPConfig();
	    String msg = 
		conf.formatMessage("null_parameter", new Object[] {param});
	    throw
		new IllegalArgumentException(msg);
	}
    }
}
