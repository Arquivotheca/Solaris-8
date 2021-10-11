/*
 * @(#)IPAddressField.java	1.1	99/03/22 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.ui;

import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import javax.swing.text.*;
import java.net.InetAddress;

import com.sun.dhcpmgr.data.IPAddress;
import com.sun.dhcpmgr.data.ValidationException;

/**
 * A text field which limits input to only those characters which can legally 
 * appear in an IP address, i.e. the digits and '.'.
 */
public class IPAddressField extends JTextField {

    /**
     * Constructs an empty field.
     */
    public IPAddressField() {
	this("");
    }
    
    /**
     * Constructs a field initialized to the provided text.
     * @param text the text to display initially
     */
    public IPAddressField(String text) {
	super(text, 15);
    }
    
    /**
     * Sets the value to a provided IP address.
     * @param addr an <code>InetAddress</code> to display
     */
    public void setValue(InetAddress addr) {
	if (addr == null) {
	    setText("");
	} else {
	    setText(addr.getHostAddress());
	}
    }
    
    /**
     * Sets the value to the provided IP address.  This is our special
     * <code>IPAddress</code> class.
     * @param addr a <code>IPAddress</code> to display
     */
    public void setValue(IPAddress addr) {
	if (addr == null) {
	    setText("");
	} else {
	    setText(addr.getHostAddress());
	}
    }
    
    /**
     * Return the current value as an <code>IPAddress</code>.
     * @return the current value as an <code>IPAddress</code>, or null if the
     *		current text is not a valid IP address.
     */
    public IPAddress getValue() {
	IPAddress a = null;
	try {
	    a = new IPAddress(getText());
	} catch (ValidationException e) {
	    // Do nothing
	}
	return a;
    }
    
    protected Document createDefaultModel() {
	return new IPAddressDocument();
    }
    
    /* 
     * This is the recommended way to validate/filter input, as opposed to
     * trapping KeyEvents because this will catch paste operations, too.
     */
    static class IPAddressDocument extends PlainDocument {
	public void insertString(int offs, String str, AttributeSet a)
	        throws BadLocationException {
	    if (str != null) {
		char [] chars = str.toCharArray();
		if ((getLength() + chars.length) > 15) {
		    // IP addresses are limited to 15 characters, period.
		    throw new BadLocationException("", offs);
		}
		for (int i = 0; i < chars.length; ++i) {
		    if (!Character.isDigit(chars[i]) && chars[i] != '.') {
			throw new BadLocationException("", offs);
		    }
		}
	    }
	    super.insertString(offs, str, a);
	}
    }
}
