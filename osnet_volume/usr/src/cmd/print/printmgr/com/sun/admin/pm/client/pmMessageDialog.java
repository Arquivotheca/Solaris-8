/*
 *
 * ident	"@(#)pmMessageDialog.java	1.3	99/03/29 SMI"
 *
 * Copyright(c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmMessageDialog.java
 * Common info message dialog
 */

package com.sun.admin.pm.client;

import javax.swing.*;
import java.util.*;
import java.awt.*;
import java.awt.event.*;
import com.sun.admin.pm.server.*;

public class pmMessageDialog extends pmDialog {

    pmButton helpButton = null;
    pmButton okButton = null;
    JList theText = null;
    pmTop theTop = null;
    String helpTag = null;


    public pmMessageDialog(String title, String msg) {
        this(null, title, msg, null, null);
    }
    
    public pmMessageDialog(Frame f, String title, String msg) {
        this(f, title, msg, null, null);
    }
    
    public pmMessageDialog(Frame f, 
			    String title, 
			    String msg, 
			    pmTop top,     
			    String h) {

        super(f, title, true);        // modal

        theTop = top;
        helpTag = h;
        
        // initialize constraints
        GridBagConstraints c = new GridBagConstraints();
        c.gridx = 0;
        c.gridy = GridBagConstraints.RELATIVE;
        c.gridwidth = 1;
        c.gridheight = 1;
        c.insets = new Insets(10, 10, 10, 10);
        c.anchor = GridBagConstraints.EAST;

        // top panel
        JPanel p = new JPanel();
        p.setLayout(new GridBagLayout());
        // p.setLayout(new BoxLayout(BoxLayout.X_AXIS));

        // JLabel label = new JLabel(msg, SwingConstants.CENTER);
        JList theText = new JList() {
            public boolean isFocusTraversable() {
                return false;
            }
        };

        Vector v = new Vector();

        Debug.message("CLNT:  MessageDialog: " + title + " , " + msg);
        
        if (msg != null) {
            StringTokenizer st = new StringTokenizer(msg, "\n", false);
            try {
                while (st.hasMoreTokens()) 
                    v.addElement(st.nextToken());
            } catch (Exception x) {
                Debug.warning("CLNT:  pmMessageDialog caught " + x);
            }
            theText.setListData(v);
        }

        theText.setBackground(p.getBackground());
        
        // p.add(theText, "Center");
        p.add(theText, c);
        
        this.getContentPane().add(p, "Center");
        
        okButton = new pmButton(
            pmUtility.getResource("Dismiss"));
        okButton.setMnemonic(
            pmUtility.getIntResource("Dismiss.mnemonic"));
        okButton.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent evt) {
                actionOKButton();
            }
        });

        // handle Esc as dismiss in any case
        this.getRootPane().registerKeyboardAction(new ActionListener() {
            public void actionPerformed(ActionEvent e) {
                Debug.message("CLNT:  default cancel action");
                actionOKButton();
            }},
            KeyStroke.getKeyStroke(KeyEvent.VK_ESCAPE, 0, false),
            JComponent.WHEN_IN_FOCUSED_WINDOW);

        p = new JPanel();
        p.add(okButton);
    
        if (theTop != null && helpTag != null) {
            helpButton = new pmButton(
                pmUtility.getResource("Help"));
            helpButton.setMnemonic(
                pmUtility.getIntResource("Help.mnemonic"));
            p.add(helpButton);
            helpButton.addActionListener(new ActionListener() {
                public void actionPerformed(ActionEvent evt) {
                    theTop.showHelpItem(helpTag);
                }   
            });
        }
        
        this.getContentPane().add(p, "South");
        this.addWindowListener(new WindowAdapter() {
            public void windowClosing(WindowEvent evt) {
                actionOKButton();
            }
        });

        this.pack();

        // this.getRootPane().setDefaultButton(okButton);
        okButton.setAsDefaultButton();
        
        // okButton.requestFocus();
        okButton.grabFocus();

    }

    
    protected void actionOKButton() {
        returnValue = JOptionPane.OK_OPTION;
        pmMessageDialog.this.setVisible(false);
    }


    public int getValue() {
        return returnValue;
    }


    public static void main(String[] args) {
        JFrame f = new JFrame("Test Dialog");
        f.setSize(300, 100);
    
        f.addWindowListener(new WindowAdapter() {
            public void windowClosing(WindowEvent evt) {
                System.exit(0);
            }
        });

        f.setVisible(true);

        while (true) {
            System.out.println("creating a new dialog instance...");
            pmMessageDialog d =
                new pmMessageDialog(null,
                                     "Dialog Test",
                                     "Dumb test message.",
				      null,
                                     null);
            d.show();
            System.out.println("Dialog returns " + d.getValue());

            d.dispose();
    
        }

    }


    protected int returnValue = JOptionPane.CLOSED_OPTION;
}
