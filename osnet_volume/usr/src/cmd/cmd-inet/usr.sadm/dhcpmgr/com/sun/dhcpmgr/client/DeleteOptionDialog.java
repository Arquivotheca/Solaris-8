/*
 * @(#)DeleteOptionDialog.java	1.6	99/08/26 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.client;

import java.awt.*;
import java.awt.event.*;
import java.text.MessageFormat;
import java.util.*;
import javax.swing.*;

import com.sun.dhcpmgr.server.*;
import com.sun.dhcpmgr.data.*;
import com.sun.dhcpmgr.ui.*;
import com.sun.dhcpmgr.bridge.NotRunningException;

/**
 * This dialog allows the user to delete an option definition
 */
public class DeleteOptionDialog extends DhcpmgrDialog {
    private JCheckBox signalBox;
    private Option option;
    private JLabel message;
    
    public DeleteOptionDialog(Frame f, Option o) {
	super(f, false);
	setTitle(ResourceStrings.getString("delete_option_title"));
	option = o;
	String [] args = new String[] { option.getKey() };
	message.setText(MessageFormat.format(
	    ResourceStrings.getString("delete_option_confirm"), args));
    }

    protected JPanel getMainPanel() {
	JPanel mainPanel = new JPanel(new BorderLayout());
	JPanel flowPanel = new JPanel();
	
	message = new JLabel();
	flowPanel.add(message);
	mainPanel.add(flowPanel, BorderLayout.NORTH);
	
	flowPanel = new JPanel();
	signalBox = new JCheckBox(ResourceStrings.getString("signal_server"),
	    true);
	flowPanel.add(signalBox);
	mainPanel.add(flowPanel, BorderLayout.CENTER);
	
	buttonPanel.setOkEnabled(true);
	return mainPanel;
    }

    protected void doOk() {
	try {
	    DhcptabMgr server = DataManager.get().getDhcptabMgr();
	    server.deleteRecord(option, signalBox.isSelected());
	    fireActionPerformed();
	    setVisible(false);
	    dispose();
	} catch (NotRunningException e) {
	    // Server not running, put up a warning
	    JOptionPane.showMessageDialog(this,
		ResourceStrings.getString("server_not_running"),
		ResourceStrings.getString("warning"),
		JOptionPane.WARNING_MESSAGE);
            fireActionPerformed();
	    setVisible(false);
	    dispose();
	} catch (Exception e) {
	    MessageFormat form = null;
	    Object [] args = new Object[2];
	    form = new MessageFormat(
		ResourceStrings.getString("delete_option_error"));
	    args[0] = option.getKey();
	    args[1] = e.getMessage();
	    JOptionPane.showMessageDialog(this, form.format(args),
		ResourceStrings.getString("server_error_title"),
		JOptionPane.ERROR_MESSAGE);
	}
    }

    protected String getHelpKey() {
	return "delete_option";
    }
    
    protected void fireActionPerformed() {
	fireActionPerformed(this, DialogActions.DELETE);
    }
}
