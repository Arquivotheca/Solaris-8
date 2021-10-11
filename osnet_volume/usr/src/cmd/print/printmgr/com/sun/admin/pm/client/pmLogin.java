/*
 *
 * ident	"@(#)pmLogin.java	1.3	99/03/29 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmLogin.java
 * Login dialog
 */

package com.sun.admin.pm.client;

import java.awt.*;
import java.awt.event.*;
import javax.swing.JPanel;
import javax.swing.*;
import com.sun.admin.pm.server.*;



/*
 * a panel dialog which captures a username and password.
 */

public class pmLogin extends pmDialog {
    private pmTop theTop = null;
    private String theTag = null;

    protected pmButton okButton = null;
    protected pmButton cancelButton = null;
    protected pmButton helpButton = null;
    
    public pmLogin(JFrame f, String title, String msg) {
        this(f, title, msg, null, null);
    }
    
    public pmLogin(JFrame f, String title, String msg, pmTop t, String h) {

        super(f, title, true);         // modal

        theTop = t;
        theTag = h;
        
        JLabel l;
        JPanel p;

        // initialize constraints
        GridBagConstraints c = new GridBagConstraints();
        c.gridx = 0;
        c.gridy = GridBagConstraints.RELATIVE;
        c.gridwidth = 1;
        c.gridheight = 1;
        c.insets = new Insets(10, 10, 10, 10);
        c.anchor = GridBagConstraints.EAST;

        // top panel contains the desired message
        p = new JPanel();
        p.setLayout(new GridBagLayout());
           
        l = new JLabel(msg, SwingConstants.CENTER);
        p.add(l, c);
        this.getContentPane().add(p, "North");
        

        // middle panel contains username and password 
        p = new JPanel();
        p.setLayout(new GridBagLayout());

        l = new JLabel(pmUtility.getResource("Hostname:"),
                        SwingConstants.RIGHT);
        p.add(l, c);

        l = new JLabel(pmUtility.getResource("Username:"),
                        SwingConstants.RIGHT);
        p.add(l, c);
    
        l = new JLabel(pmUtility.getResource("Password:"),
                        SwingConstants.RIGHT);
        p.add(l, c);

        passwordField = new JPasswordField(12);
        passwordField.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent e) {
                okPressed();
            }
        });
        l.setLabelFor(passwordField);
        // for consistency, don't implement this until all are...
        // l.setDisplayedMnemonic(
	// 	pmUtility.getIntResource("Password.mnemonic"));

        c.gridx = 1;
        c.weightx = 1.0;

        String nisMaster;
        try {
            nisMaster = theTop.host.getNisMaster();
        } catch (Exception e) {
            nisMaster = new String("Unknown");
            Debug.warning("pmLogin: getNisMaster() returns exception: " + e);
        }

        c.anchor = GridBagConstraints.WEST;

        l = new JLabel(nisMaster, SwingConstants.LEFT);
        p.add(l, c);

        l = new JLabel(("root"), SwingConstants.LEFT);
        p.add(l, c);


        c.fill = GridBagConstraints.HORIZONTAL;
        c.anchor = GridBagConstraints.CENTER;
        c.gridy = GridBagConstraints.RELATIVE;

        p.add(passwordField, c);
        passwordField.setEchoChar('*');
        
        this.getContentPane().add(p, "Center");

        // bottom panel contains buttons
        c.gridx = 0;
        c.weightx = 1.0;
        c.weighty = 0.0;
        c.gridwidth = GridBagConstraints.REMAINDER;
        c.fill = GridBagConstraints.HORIZONTAL;
        c.anchor = GridBagConstraints.CENTER;

        JPanel thePanel = new JPanel();

        okButton = new pmButton(
            pmUtility.getResource("OK"));
        okButton.setMnemonic(
            pmUtility.getIntResource("OK.mnemonic"));
        okButton.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent evt) {
                okPressed();
            }
        });
        thePanel.add(okButton, c);

        cancelButton = new pmButton(
            pmUtility.getResource("Cancel"));
        cancelButton.setMnemonic(
            pmUtility.getIntResource("Cancel.mnemonic"));
        cancelButton.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent evt) {
                cancelPressed();
            }
        });
        thePanel.add(cancelButton, c);

        if (theTag != null && theTop != null) {

            helpButton = new pmButton(
                pmUtility.getResource("Help"));
            helpButton.setMnemonic(
                pmUtility.getIntResource("Help.mnemonic"));
            p.add(helpButton);
            helpButton.addActionListener(new ActionListener() {
                public void actionPerformed(ActionEvent evt) {
					helpPressed();
                }   
            });
            thePanel.add(helpButton, c);
        }

        this.getContentPane().add(thePanel, "South");

        addWindowListener(new WindowAdapter() {
            public void windowClosing(WindowEvent e) {
                returnValue = JOptionPane.CLOSED_OPTION;
                pmLogin.this.setVisible(false);
            }
        });

        // handle Esc as cancel in any case
        this.getRootPane().registerKeyboardAction(new ActionListener() {
            public void actionPerformed(ActionEvent e) {
                Debug.message("CLNT:  default cancel action");
                cancelPressed();
            }},
            KeyStroke.getKeyStroke(KeyEvent.VK_ESCAPE, 0, false),
            JComponent.WHEN_IN_FOCUSED_WINDOW);
       
        // lay out the dialog
        this.pack();

        // set focus and defaults after packing...
        // this.getRootPane().setDefaultButton(okButton);
        okButton.setAsDefaultButton();
        
        passwordField.requestFocus();
    }

    public int getValue() {
        return returnValue;
    }


    public void okPressed() {
        returnValue = JOptionPane.OK_OPTION;
        pmLogin.this.setVisible(false);
    }

    public void cancelPressed() {
       	returnValue = JOptionPane.CANCEL_OPTION;
       	pmLogin.this.setVisible(false);
    }


    public void clearPressed() {

        passwordField.setText("");
    }

    public void helpPressed() {
        theTop.showHelpItem(theTag);
    }
    
    public static void main(String[] args) {
        JFrame f = new JFrame("Password test");

        f.setSize(300, 100);
        f.addWindowListener(new WindowAdapter() {
            public void windowClosing(WindowEvent e) {
                System.exit(0);
            }
        });
        f.setVisible(true);

	while (true) {
	    pmLogin d = new pmLogin(f, "Test Login", "NIS Authentication.");
        d.show();


    }
     // System.exit(0);
    }

    public JPasswordField passwordField = null;

    protected int returnValue = JOptionPane.CLOSED_OPTION;
    
}
                
    
    
    
 

