/*
 * @(#)UnconfigureDialog.java	1.6	99/10/12 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.client;

import javax.swing.*;
import javax.swing.border.*;
import javax.swing.event.*;
import javax.swing.table.*;

import java.awt.*;
import java.awt.event.*;
import java.text.*;
import java.util.*;
import java.net.*;

import com.sun.dhcpmgr.server.*;
import com.sun.dhcpmgr.data.*;
import com.sun.dhcpmgr.ui.*;

/**
 * A dialog to confirm the user's request to unconfigure the service.
 */
public class UnconfigureDialog extends MultipleOperationDialog {
    private JCheckBox deleteTables, deleteHosts;
    private int networkCount = 0;
    private Network [] nets = new Network[0];

    public UnconfigureDialog(Frame f) {
	// No reset button for us
	super(f, false);
    }

    public String getTitle() {
	return ResourceStrings.getString("unconfigure_title");
    }

    protected JPanel getMainPanel() {
	
	JPanel mainPanel = new JPanel();
	mainPanel.setBorder(BorderFactory.createEmptyBorder(10, 10, 10, 10));
	
	Box box = Box.createVerticalBox();
	
	if (!DhcpmgrApplet.modeIsRelay) {
	    JComponent c = Wizard.createTextArea(
		ResourceStrings.getString("unconfigure_dhcp"), 4, 30);
	    c.setAlignmentX(Component.LEFT_ALIGNMENT);
	    box.add(c);
	    box.add(Box.createVerticalStrut(10));
	    deleteTables = new JCheckBox(
		ResourceStrings.getString("unconfigure_delete_tables"), false);
	    deleteTables.setAlignmentX(Component.LEFT_ALIGNMENT);
	    box.add(deleteTables);
	    box.add(Box.createVerticalStrut(10));
	    deleteHosts = new JCheckBox(
		ResourceStrings.getString("unconfigure_delete_hosts"), false);
	    deleteHosts.setAlignmentX(Component.LEFT_ALIGNMENT);
	    deleteHosts.setEnabled(false);
	    box.add(deleteHosts);
	    deleteTables.addActionListener(new ActionListener() {
		public void actionPerformed(ActionEvent e) {
		    deleteHosts.setEnabled(deleteTables.isSelected());
		}
	    });
	} else {
	    JComponent c = Wizard.createTextArea(
		ResourceStrings.getString("unconfigure_bootp"), 4, 30);
	    c.setAlignmentX(Component.LEFT_ALIGNMENT);
	    box.add(c);
	}
	box.add(Box.createVerticalStrut(10));
	JComponent c = Wizard.createTextArea(
	    ResourceStrings.getString("unconfigure_shutdown"), 2, 30);
	c.setAlignmentX(Component.LEFT_ALIGNMENT);
	box.add(c);
	mainPanel.add(box);
	buttonPanel.setOkEnabled(true);
    	return mainPanel;
    }
    
    protected String getProgressMessage() {
        return ResourceStrings.getString("unconfigure_progress");
    }

    protected int getProgressLength() {
	// Initialize to number of ops required even if only a relay
    	int length = 3;
	if (!DhcpmgrApplet.modeIsRelay) {
	    // Add one for deleting defaults file, and one for deleting macro
	    length += 2;
	    if (deleteTables.isSelected()) {
	    	try {
	            nets = DataManager.get().getNetworks(false);
	    	} catch (Throwable t) {
	    	    // Ignore
	    	}
	    	length += nets.length + 1; // Add one for dhcptab
	    }
	}
	return length;
    }

    protected String getErrorHeading() {
    	return ResourceStrings.getString("unconfigure_error_heading");
    }

    protected Thread getOperationThread() {
	return new Thread() {
	    public void run() {
	    	int checkpoint = 0;
	        DhcpServiceMgr serviceMgr =
		    DataManager.get().getDhcpServiceMgr();
	        // Shut down the server
	        try {
		    serviceMgr.shutdown();
	        } catch (Throwable e) {
		    addError(
		    	ResourceStrings.getString("unconfigure_error_shutdown"),
			e.getMessage());
	        }
		try {
		    updateProgress(++checkpoint, ResourceStrings.getString(
		        "unconfigure_server_shutdown"));
		} catch (InterruptedException e) {
		    closeDialog();
		    return;
		}

		// Remove the defaults file
		try {
		    serviceMgr.removeDefaults();
		} catch (Throwable e) {
		    addError(ResourceStrings.getString(
			"unconfigure_error_defaults"), e.getMessage());
		}
		try {
		    updateProgress(++checkpoint, ResourceStrings.getString(
			"unconfigure_defaults_deleted"));
		} catch (InterruptedException e) {
		    closeDialog();
		    return;
		}

		// Remove the boot script links
	    	try {
		    serviceMgr.removeLinks();
	    	} catch (Throwable e) {
		    addError(
			ResourceStrings.getString("unconfigure_error_links"),
			e.getMessage());
		}
		try {
		    updateProgress(++checkpoint, ResourceStrings.getString(
		        "unconfigure_links_deleted"));
		} catch (InterruptedException e) {
		    closeDialog();
		    return;
		}

	    	// If this was a relay we're done
	    	if (!DhcpmgrApplet.modeIsRelay) {
		    // Remove the server macro
		    try {
		    	DataManager.get().getDhcptabMgr().deleteRecord(
			    new Macro(DataManager.get().getShortServerName()),
			    false);
		    } catch (Throwable e) {
			addError(ResourceStrings.getString(
			    "unconfigure_error_macro"),
		    	    e.getMessage());
		    }
		    try {
		        updateProgress(++checkpoint, ResourceStrings.getString(
			    "unconfigure_macro_deleted"));
		    } catch (InterruptedException e) {
			closeDialog();
		    	return;
		    }

		    // Delete all the network tables and the dhcptab
		    if (deleteTables.isSelected()) {
		        if (nets != null && nets.length != 0) {
			    MessageFormat errForm = new MessageFormat(
			        ResourceStrings.getString(
				"unconfigure_error_network"));
			    MessageFormat progForm = new MessageFormat(
			        ResourceStrings.getString(
				"unconfigure_network_progress"));
			    Object [] args = new Object[1];
			    for (int i = 0; i < nets.length; ++i) {
				String netString = nets[i].toString();
				args[0] = netString;
			        try {
				    DataManager.get().getDhcpNetMgr().
				        deleteNetwork(netString,
				        deleteHosts.isSelected());
			        } catch (Throwable e) {
			            addError(errForm.format(args),
				        e.getMessage());
				}
				try {
				    updateProgress(++checkpoint,
				        progForm.format(args));
			    	} catch (InterruptedException e) {
				    closeDialog();
				    return;
				}
			    }
		        }
		    	try {
			    DataManager.get().getDhcptabMgr().deleteDhcptab();
		    	} catch (Throwable e) {
			    addError(ResourceStrings.getString(
			    	"unconfigure_error_dhcptab"),
				e.getMessage());
		    	}
			try {
			    updateProgress(++checkpoint,
			        ResourceStrings.getString(
				"unconfigure_dhcptab_deleted"));
		        } catch (InterruptedException e) {
			    closeDialog();
			    return;
			}
		    }
	        }
	        if (errorsOccurred()) {
		    displayErrors(ResourceStrings.getString(
		        "unconfigure_error_messages"));
		}
    	    	closeDialog();
	    }
	};
    }

    protected String getHelpKey() {
        if (DhcpmgrApplet.modeIsRelay) {
	    return "unconfigure_relay";
	} else {
	    return "unconfigure_server";
	}
    }
    
    protected void fireActionPerformed() {
	fireActionPerformed(this, DialogActions.OK);
    }

}
