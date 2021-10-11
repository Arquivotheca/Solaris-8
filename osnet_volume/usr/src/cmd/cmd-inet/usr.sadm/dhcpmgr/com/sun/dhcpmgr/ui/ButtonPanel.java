/*
 * @(#)ButtonPanel.java	1.2	99/05/18 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.ui;

import javax.swing.*;
import java.awt.event.*;
import java.util.*;

/**
 * A simple panel with the buttons commonly used in a dialog.  Register as
 * a ButtonPanelListener in order to receive the events from this class.
 * @see ButtonPanelListener
 */
public class ButtonPanel extends JPanel {

    // Convert action events on each button to button panel notifications 
    class ButtonAdaptor implements ActionListener {
	public void actionPerformed(ActionEvent e) {
	    int buttonId = -1;
	    Object source = e.getSource();
	    if (source == okButton) {
		buttonId = ButtonPanelListener.OK;
	    } else if (source == resetButton) {
		buttonId = ButtonPanelListener.RESET;
	    } else if (source == cancelButton) {
		buttonId = ButtonPanelListener.CANCEL;
	    } else if (source == helpButton) {
		buttonId = ButtonPanelListener.HELP;
	    }
	    Enumeration en = listeners.elements();
	    while (en.hasMoreElements()) {
		ButtonPanelListener l = (ButtonPanelListener)en.nextElement();
		l.buttonPressed(buttonId);
	    }
	}
    }
    
    JButton okButton, resetButton, cancelButton, helpButton;
    ButtonAdaptor adaptor;
    Vector listeners;
    
    /**
     * Construct a ButtonPanel with OK, Cancel, and Help buttons, and
     * the reset button display controlled by the parameter.
     * @param showReset true if Reset button is to be included
     */
    public ButtonPanel(boolean showReset) {
    	this(showReset, true);
    }

    /**
     * Construct a ButtonPanel with reset and help buttons controlled
     * by the parameters passed, and always showing OK and Cancel.
     * @param showReset true if Reset button is to be included
     * @param showHelp true if Help button is to be included
     */
    public ButtonPanel(boolean showReset, boolean showHelp) {
	super();
	setLayout(new ButtonLayout(ALIGNMENT.RIGHT));
	// Create event handler
	adaptor = new ButtonAdaptor();
	listeners = new Vector();
	
	okButton = new JButton(ResourceStrings.getString("ok_button"));
	okButton.setEnabled(false);
	okButton.addActionListener(adaptor);
	add(okButton);
	
	// Only show reset if requested
	if (showReset) {
	    resetButton = new JButton(
		ResourceStrings.getString("reset_button"));
	    resetButton.addActionListener(adaptor);
	    add(resetButton);
	} else {
	    resetButton = null;
	}
	
	cancelButton = new JButton(ResourceStrings.getString("cancel_button"));
	cancelButton.addActionListener(adaptor);
	add(cancelButton);
	
	if (showHelp) {
	    helpButton = new JButton(ResourceStrings.getString("help_button"));
	    helpButton.addActionListener(adaptor);
	    add(helpButton);
	} else {
	    helpButton = null;
	}
    }
    
    public void addButtonPanelListener(ButtonPanelListener l) {
	listeners.addElement(l);
    }
    
    public void removeButtonPanelListener(ButtonPanelListener l) {
	listeners.removeElement(l);
    }
    
    public void setOkEnabled(boolean state) {
	okButton.setEnabled(state);
    }
    
    public boolean isOkEnabled() {
	return okButton.isEnabled();
    }
	
}
