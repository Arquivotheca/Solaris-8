/*
 * @(#)OptionNameField.java	1.1	99/04/27 SMI
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
 * A text field which enforces the syntax rules for an option name.  These
 * are all the rules for DhcptabNameField, plus a limit of 8 characters
 * in length.
 */
public class OptionNameField extends DhcptabNameField {
    private static int MAX_LENGTH = 8;
    /**
     * Constructs a field initialized to the provided text.  Defaults to
     * 10 characters wide.
     * @param text the text to display initially
     */
    public OptionNameField(String text) {
	this(text, 10);
    }
    
    /**
     * Constructs a field initialized to the provided text with the requested
     * size.
     * @param text the text to display initially
     * @param length the length in characters the field should size itself to
     */
    public OptionNameField(String text, int length) {
	super(text, length);
    }
    
    protected Document createDefaultModel() {
	return new OptionNameDocument();
    }
    
    /* 
     * This is the recommended way to validate input, as opposed to trapping
     * KeyEvents because this will actually catch paste operations as well.
     */
    class OptionNameDocument extends DhcptabNameDocument {
	public void insertString(int offs, String str, AttributeSet a)
		throws BadLocationException {
	    if (str != null) {
		if ((getLength() + str.length()) > MAX_LENGTH) {
		    throw new BadLocationException("", offs);
		}
	    }
	    super.insertString(offs, str, a);
	}
    }
}
