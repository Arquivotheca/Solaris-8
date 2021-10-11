/*
 *
 * ident	"@(#)pmOKCancelDialog.java	1.3	99/03/29 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmOKCancelDialog.java
 * Common dialog
 */

package com.sun.admin.pm.client;

import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import com.sun.admin.pm.server.Debug;


public class pmOKCancelDialog extends pmDialog {
    private pmTop theTop;
    private String theTag;
    protected boolean defaultIsOK = true;
    
    public pmOKCancelDialog(Frame f, String title, String msg) {
        this(f, title, msg, null, null, true);
    }

    public pmOKCancelDialog(Frame f, String title, String msg, boolean ok) {
        this(f, title, msg, null, null, ok);
    } 

    public pmOKCancelDialog(Frame f, String title, String msg,
                             pmTop t, String h) {
        this(f, title, msg, t, h, true);
    }
    
    public pmOKCancelDialog(Frame f, String title, String msg,
                             pmTop t, String h, boolean ok) {
        super(f, title, true);		// modal

        theTop = t;
        theTag = h;
        defaultIsOK = ok;
        
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

        JLabel label = new JLabel(msg, SwingConstants.CENTER);
        p.add(label, c);

        this.getContentPane().add(p, "Center");
        
        this.getContentPane().add(
            buttonPanel(defaultIsOK, theTop != null && theTag != null),
            "South");

        this.pack();
        
        this.addWindowListener(new WindowAdapter() {
            public void windowClosing(WindowEvent evt) {
                returnValue = JOptionPane.CLOSED_OPTION;
                pmOKCancelDialog.this.setVisible(false);
            }
        });

        if (defaultIsOK) {
            // this.getRootPane().setDefaultButton(okButton);
            okButton.setAsDefaultButton();
            okButton.requestFocus();
        } else {
            // this.getRootPane().setDefaultButton(cancelButton);
            cancelButton.setAsDefaultButton();
            cancelButton.requestFocus();
        }

        // handle Esc as cancel in any case
        this.getRootPane().registerKeyboardAction(new ActionListener() {
            public void actionPerformed(ActionEvent e) {
                Debug.message("CLNT:  default cancel action");
                actionCancelButton();
            }},
            KeyStroke.getKeyStroke(KeyEvent.VK_ESCAPE, 0, false),
            JComponent.WHEN_IN_FOCUSED_WINDOW);
    }

    public JPanel buttonPanel(boolean okDefault, boolean useHelp) {
        JPanel panel = new JPanel();

        panel.setLayout(new GridBagLayout());
        GridBagConstraints c = new GridBagConstraints();

        c.gridheight = 1;
        c.gridwidth = 1;
        c.weightx = c.weighty = 0.0;
        c.anchor = GridBagConstraints.CENTER;
        c.fill = GridBagConstraints.HORIZONTAL;
        c.gridy = 0;


	if (okDefault) 
            c.gridx = 0;
        else
            c.gridx = 1;

        okButton = new pmButton(
            pmUtility.getResource("OK"));
        okButton.setMnemonic(
            pmUtility.getIntResource("OK.mnemonic"));

	if (okDefault) 
            c.gridx = 1;
        else
            c.gridx = 0;

        cancelButton = new pmButton(
            pmUtility.getResource("Cancel"));
        cancelButton.setMnemonic(
            pmUtility.getIntResource("Cancel.mnemonic"));

        helpButton = null;
            
        if (useHelp) {
            c.gridx = 2;
            helpButton = new pmButton(
                pmUtility.getResource("Help"));
            helpButton.setMnemonic(
                pmUtility.getIntResource("Help.mnemonic"));
        }

        okButton.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent evt) {
                actionOKButton();
            }
        });

        cancelButton.addActionListener(new ActionListener() {
            public void actionPerformed(ActionEvent evt) {
                actionCancelButton();
            }
        });

        if (helpButton != null) {
            helpButton.addActionListener(new ActionListener() {
                public void actionPerformed(ActionEvent evt) {
                    theTop.showHelpItem(theTag);
                }   
            });
        }

        c.insets = new Insets(15, 15, 15, 15);
        c.gridx = 0;
        panel.add(okButton, c);
        c.gridx = 1;
        panel.add(cancelButton, c);
        c.gridx = 2;
        if (helpButton != null)
            panel.add(helpButton, c);

        return panel;
    }

    protected void actionOKButton() {
        returnValue = JOptionPane.OK_OPTION;
        pmOKCancelDialog.this.setVisible(false);
    }

    protected void actionCancelButton() {
        returnValue = JOptionPane.CANCEL_OPTION;
        pmOKCancelDialog.this.setVisible(false);
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
            pmOKCancelDialog d =
                new pmOKCancelDialog(
			null, "Dialog Test", "Some message.", false);
            d.show();
            System.out.println("Dialog returns " + d.getValue());

            d.dispose();
    
        }

    }


    pmButton helpButton = null;
    pmButton okButton = null;
    pmButton cancelButton = null;
    
    protected int returnValue = JOptionPane.CLOSED_OPTION;
}

