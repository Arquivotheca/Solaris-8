/*
 *
 * ident	"@(#)pmHelpFrame.java	1.2	99/03/29 SMI"
 *
 * Copyright(c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmHelpFrame.java
 * Container for help subsystem GUI
 */

package com.sun.admin.pm.client;

import java.lang.*;
import java.awt.*;
import java.awt.event.*;
import java.util.*;
import java.io.*;
import javax.swing.JPanel;
import javax.swing.border.*;
import javax.swing.*;
import com.sun.admin.pm.server.*;


public class pmHelpFrame extends pmFrame {
    
    protected pmHelpController theController = null;
    public pmButton dismiss = null;  // expose for default button hacks
    
    public pmHelpFrame() {
        super(pmUtility.getResource("SPM:Help"));

        theController = new pmHelpController(this);
        getContentPane().add("Center", theController.getTopPane());
        
        dismiss = new pmButton(
            pmUtility.getResource("Dismiss"));
        dismiss.setMnemonic(
            pmUtility.getIntResource("Dismiss.mnemonic"));
        dismiss.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent e) {
                hideHelp();
            }
        });

        JPanel p = new JPanel();
        p.add(dismiss);

        getContentPane().add("South", p);
    
        this.pack();
        this.setVisible(false);
        this.repaint();

        // default button is dismiss
        // getRootPane().setDefaultButton(dismiss);
        dismiss.setAsDefaultButton();
    
        // handle Esc as dismiss
        getRootPane().registerKeyboardAction(new ActionListener() {
            public void actionPerformed(ActionEvent e) {
                Debug.message("HELP:  dismiss action");
                hideHelp();
            }},
            KeyStroke.getKeyStroke(KeyEvent.VK_ESCAPE, 0, false),
            JComponent.WHEN_IN_FOCUSED_WINDOW);
    }


    public void hideHelp() {
        this.setVisible(false);
    }

    
    public void showHelp(String tag) {
        theController.showHelpItem(tag);
        this.setVisible(true);
        this.repaint();
    }
        
}

