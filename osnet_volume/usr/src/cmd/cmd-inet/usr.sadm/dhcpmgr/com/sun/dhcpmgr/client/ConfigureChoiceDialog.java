/*
 * @(#)ConfigureChoiceDialog.java	1.4	99/08/05 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.client;

import javax.swing.*;
import javax.swing.border.*;
import java.awt.event.*;
import java.awt.*;

import com.sun.dhcpmgr.ui.*;

/**
 * Dialog to select which type of server we want: full DHCP or just a BOOTP
 * relay. This is implemented as a singleton modal so the caller can't do
 * anything until it returns a selection.
 */
public class ConfigureChoiceDialog extends JDialog
	implements ButtonPanelListener {
    private ButtonPanel buttonPanel;
    private ButtonGroup buttonGroup;
    private JRadioButton dhcp, bootp;
    /**
     * Returned if user decides choice is "none of the above"
     */
    public static final int CANCELLED = 0;
    /**
     * Return value if user wants DHCP service
     */
    public static final int DHCP = 1;
    /**
     * Return value if user wants BOOTP relay
     */
    public static final int BOOTP = 2;
    private static int value = CANCELLED;
    
    // Must use the showDialog method to get one of these.
    private ConfigureChoiceDialog(Frame f) {
	super(f, true);
	setTitle(ResourceStrings.getString("configure_choice_title"));
	setLocationRelativeTo(f);
	
	getContentPane().setLayout(new BorderLayout());
	
	JPanel panel = new JPanel(new BorderLayout());
	panel.setBorder(
	    BorderFactory.createCompoundBorder(
	    BorderFactory.createEtchedBorder(), 
	    BorderFactory.createEmptyBorder(10, 20, 10, 20)));
	
	// Explanatory text at the top
	panel.add(Wizard.createTextArea(
	    ResourceStrings.getString("configure_choice_explain"), 5, 30), 
	    BorderLayout.NORTH);
	
	// Just show the choices as a set of radio buttons
	buttonGroup = new ButtonGroup();
	dhcp = new JRadioButton(
	    ResourceStrings.getString("configure_dhcp_server"), true);
	buttonGroup.add(dhcp);
	Box box = Box.createVerticalBox();
	box.add(dhcp);
	box.add(Box.createVerticalStrut(5));
	bootp = new JRadioButton(
	    ResourceStrings.getString("configure_bootp_relay"), false);
	buttonGroup.add(bootp);
	box.add(bootp);
	panel.add(box, BorderLayout.SOUTH);
	getContentPane().add(panel, BorderLayout.NORTH);
	
	buttonPanel = new ButtonPanel(false);
	buttonPanel.addButtonPanelListener(this);
	buttonPanel.setOkEnabled(true);
	getContentPane().add(buttonPanel, BorderLayout.SOUTH);
    }
    
    public void buttonPressed(int buttonId) {
	switch (buttonId) {
	case OK:
	    if (dhcp.isSelected()) {
		value = DHCP;
	    } else {
		value = BOOTP;
	    }
	    setVisible(false);
	    break;
	case CANCEL:
	    value = CANCELLED;
	    setVisible(false);
	    break;
	case HELP:
	    DhcpmgrApplet.showHelp("server_config");
	    break;
	}
    }
    
    public static int showDialog(Frame f) {
	ConfigureChoiceDialog d = new ConfigureChoiceDialog(f);
	d.pack();
	d.show();
	d.dispose();
	return value;
    }
}
