/*
 * @(#)DhcptabNameField.java	1.1	99/04/27 SMI
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.client;

import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import javax.swing.text.*;

/**
 * A text field which enforces the syntax rules for the name field in the
 * dhcptab.  At present, those rules are:
 *	1. All characters must be NVT ASCII
 *	2. Whitespace and # characters are not allowed
 */
public class DhcptabNameField extends JTextField {

    /**
     * Constructs a field initialized to the provided text.  Defaults to
     * 20 characters wide.
     * @param text the text to display initially
     */
    public DhcptabNameField(String text) {
	this(text, 20);
    }
    
    /**
     * Constructs a field initialized to the provided text with the requested
     * size.
     * @param text the text to display initially
     * @param length the length in characters the field should size itself to
     */
    public DhcptabNameField(String text, int length) {
	super(text, length);
    }
    
    protected Document createDefaultModel() {
	return new DhcptabNameDocument();
    }
}    

/** 
 * This is the recommended way to validate input, as opposed to trapping
 * KeyEvents because this will actually catch paste operations as well.
 */
class DhcptabNameDocument extends PlainDocument {
    public void insertString(int offs, String str, AttributeSet a)
	    throws BadLocationException {
    	if (str != null) {
	    char [] chars = str.toCharArray();
	    for (int i = 0; i < chars.length; ++i) {
	   	if (chars[i] < 33 || chars[i] > 127 || chars[i] == '#') {
		    throw new BadLocationException("", offs);
	    	}
	    }
    	}
    	super.insertString(offs, str, a);
    }
}
