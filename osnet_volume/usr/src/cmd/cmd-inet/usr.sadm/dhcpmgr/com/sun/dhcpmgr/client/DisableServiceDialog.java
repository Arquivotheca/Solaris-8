/*
 * @(#)DisableServiceDialog.java	1.4	99/08/27 SMI
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

/**
 *  This dialog handles enabling and disabling the service, with user
 * confirmation
 */
public class DisableServiceDialog extends DhcpmgrDialog {
    private boolean disable;
    private JLabel message;
    
    public DisableServiceDialog(Frame f, boolean disable) {
	super(f, false);
	
	this.disable = disable;
	
	String [] args = new String[] { 
	    DataManager.get().getShortServerName() };
	if (disable) {
	    setTitle(ResourceStrings.getString("disable_service_title"));
	    if (DhcpmgrApplet.modeIsRelay) {
		message.setText(MessageFormat.format(
		    ResourceStrings.getString("disable_relay_confirm"), args));
	    } else {
		message.setText(MessageFormat.format(
		    ResourceStrings.getString("disable_service_confirm"),
		    args));
	    }
	} else {
	    setTitle(ResourceStrings.getString("enable_service_title"));
	    if (DhcpmgrApplet.modeIsRelay) {
		message.setText(MessageFormat.format(
		    ResourceStrings.getString("enable_relay_confirm"), args));
	    } else {
		message.setText(MessageFormat.format(
		    ResourceStrings.getString("enable_service_confirm"), args));
	    }
	}
    }

    protected JPanel getMainPanel() {
	JPanel panel = new JPanel();
	panel.setBorder(BorderFactory.createEmptyBorder(20, 10, 20, 10));
	message = new JLabel();
	panel.add(message);

	buttonPanel.setOkEnabled(true);
	return panel;
    }

    protected void doOk() {
	try {
	    DhcpServiceMgr server = DataManager.get().getDhcpServiceMgr();
	    if (disable) {
		// Shutdown the server and delete links to the boot script
		server.shutdown();
		server.removeLinks();
	    } else {
		// Enable = reverse the process
		server.createLinks();
		server.startup();
	    }
	    fireActionPerformed();
	    setVisible(false);
	    dispose();
	} catch (Exception e) {
	    e.printStackTrace();
	    MessageFormat form = null;
	    Object [] args = new Object[1];
	    if (disable) {
		form = new MessageFormat(
		    ResourceStrings.getString("disable_service_error"));
	    } else {
		form = new MessageFormat(
		    ResourceStrings.getString("enable_service_error"));
	    }
	    args[0] = e.getMessage();
	    JOptionPane.showMessageDialog(this, form.format(args),
		ResourceStrings.getString("server_error_title"),
		JOptionPane.ERROR_MESSAGE);
	}
    }

    protected String getHelpKey() {
	if (disable) {
	    return "disable_service";
	} else {
	    return "enable_service";
	}
    }
    
    /**
     * Notify listeners that enable or disable has been done.
     */
    protected void fireActionPerformed() {
	String cmd;
	if (disable) {
	    cmd = DialogActions.DISABLE;
	} else {
	    cmd = DialogActions.ENABLE;
	}
	fireActionPerformed(this, cmd);
    }
}
