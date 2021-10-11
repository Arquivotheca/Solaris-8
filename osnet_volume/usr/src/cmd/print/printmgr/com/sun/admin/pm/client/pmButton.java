/*
 *
 * ident	"@(#)pmButton.java	1.3	99/09/24 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmButton.java
 * 
 */

package com.sun.admin.pm.client;

import java.util.*;
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;

import com.sun.admin.pm.server.*;

/* 
 * strategy:
 * Keep a hashtable of root panes and their associated default buttons.
 * Note that there is at present no way to remove a root pane entry
 * from the table...
 *
 * Ideally there should be an interface to allow objects to 
 * remove themselves before disappearing.
 */

public class pmButton extends JButton {

    // static JButton defaultButton = null;

    // map root panes to their true default buttons
    static Hashtable map = new Hashtable();

    public static Hashtable getHashtable() {
        return map;
    }

    /*
     * make this button the default on this root pane
     * retunrs true if success, false o/w
     */
    boolean makeDefaultButton() {
        return makeDefaultButton(this);
    }
    
    /*
     * make b the default on this root pane
     * returns true if success, false otherwise
     */
    boolean makeDefaultButton(JButton b) {
        JRootPane r = this.getRootPane();

        if (r == null) {
            Debug.info("BUTTON:  null root panel");
            return false;
        }

        if (b == null) {
            Debug.info("BUTTON:  makeDefaultButton null on " + r);
        }

        /*
         * Debug.info("\nBUTTON:  makeDefaultButton " +
         *         (b == null ? "null" : b.getText()) +
         *              " on " + r + "\n");
         */

        if (b != null && b.isDefaultCapable() == false) {
            Debug.info("BUTTON:  false isDefaultCapable on " + r);
            return false;
        }

        // unfocus the old default, if it's different
        JButton oldb;
        if ((oldb = r.getDefaultButton()) != null && oldb != b) {
            oldb.setFocusPainted(false);
        }

        /*
         * Debug.info("\nBUTTON:  makeDefaultButton: old button was " + 
         *              (oldb == null ? "null" : oldb.getText()) + "\n");
         */
 
        r.setDefaultButton(b);

        return true;
    }

    
    public pmButton(String s) {
        super(s);

        this.addFocusListener(new FocusAdapter() {

            // upon gaining focus: make this the root pane's default
            public void focusGained(FocusEvent e) {
                if (e.isTemporary()) {
                    /*
                     * Debug.info("BUTTON:  " + getText() +
                     *             " gained temp - ignoring");
                     */
                    return;
                }

                Debug.info("BUTTON:  " + getText() + " gained");

                if (makeDefaultButton())
                    setFocusPainted(true);
                
            }

            // upon losing focus: make 'true' default the default
            public void focusLost(FocusEvent e) {
                if (e.isTemporary()) {
                    /*
                     * Debug.info("BUTTON:  " + getText() +
                     *              " lost temp - ignoring");
                     */
                    return;
                }

                Debug.info("BUTTON:  " + getText() + " lost");

                /*
                 * i thought it might make sense to test for the
                 * next focusable comp, but what if focus is being
                 * lost as the result of a mouse click??
                 */
                
                makeDefaultButton((JButton) map.get(getRootPane()));
                // setFocusPainted(false);
            }
                        
        });
    }

    // make this the true default for this root pane
    void setAsDefaultButton() {
        setAsDefaultButton(this);
    }
    
    // make b the true default for this root pane
    void setAsDefaultButton(JButton b) {
        JRootPane r = getRootPane();

        /*
         * Debug.message("BUTTON:  setAsDefaultButton " +
         *            (b == null ? "null" : b.getText()) +
         *                   " root = " + r);
         */

        // setting default to null removes state
        if (b == null)
            map.remove(r);
        else
            map.put(r, b);         // creates a new entry if needed
        makeDefaultButton(b);
    }


    // clean up component about to be removed
    void unreference() {
        JRootPane r = getRootPane();
        map.remove(r);
    }
    
    public static void unreference(JComponent c) {
        JRootPane r = c.getRootPane();
        map.remove(r);
    }
    
    public static void unreference(JRootPane r) {
        map.remove(r);
    }
    

    static boolean enableMnemonics = false;

    static void setEnableMnemonics(boolean m) {
        enableMnemonics = m;
    }

    public void setMnemonic(int mnemonic) {
        setMnemonic((char)mnemonic);    
    }

    public void setMnemonic(char mnemonic) {
        if (enableMnemonics)
            super.setMnemonic(mnemonic);
    }

}




