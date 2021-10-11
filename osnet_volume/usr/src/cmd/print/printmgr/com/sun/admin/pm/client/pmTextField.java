/*
 *
 * ident	"@(#)pmTextField.java	1.3	99/03/29 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmTextField.java
 * Extension of JTextField which accepts only 8-bit-ASCII.
 */

package com.sun.admin.pm.client;

import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import javax.swing.text.*;

public class pmTextField extends JTextField {
    public pmTextField(int n) {
        this(null, n);
    }

    public pmTextField(String s) {
        this(s, 0);
    }

    public pmTextField(String s, int n) {
        super(s, n);
    }

    protected Document createDefaultModel() {
        return new pmFilterDoc();
    }

    /*
     * This doc implementation will disallow insertion of a 
     * string containing any characters which are non-8-bit-ascii.
     */
    private class pmFilterDoc extends PlainDocument {
        public void insertString(int offset, String str, AttributeSet a)
            throws BadLocationException {
            int i, c;
            char[] buf = str.toCharArray();

            for (i = 0; i < buf.length; i++) {
                c = (new Character(buf[i])).charValue();
                if (c > 0x00ff)
                    break;
            }
            if (i == buf.length)
                super.insertString(offset, str, a);
            else
                Toolkit.getDefaultToolkit().beep();
    }
    }

    public static void main(String args[]) {
        JFrame f = new JFrame();
        f.getContentPane().add(new pmTextField(20));
        f.pack();
        f.setVisible(true);
        f.repaint();
    }

}

