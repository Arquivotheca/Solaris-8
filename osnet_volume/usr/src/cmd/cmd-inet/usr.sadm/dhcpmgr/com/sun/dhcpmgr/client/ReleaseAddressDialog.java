/*
 * @(#)ReleaseAddressDialog.java	1.4	99/06/03 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.client;

import java.awt.*;
import java.awt.event.*;
import java.text.*;
import java.util.*;
import javax.swing.*;
import javax.swing.table.*;

import com.sun.dhcpmgr.server.*;
import com.sun.dhcpmgr.data.*;
import com.sun.dhcpmgr.ui.*;

/**
 * This dialog is used to release one or more addresses.  Release is defined
 * to mean resetting the client id to 00 and setting the lease date to 0.
 */
public class ReleaseAddressDialog extends MultipleOperationDialog {
    private DhcpClientRecord [] recs;
    private String table;
    boolean showAddresses;
    
    class AddressTableModel extends AbstractTableModel {

	public int getRowCount() {
	    return recs.length;
	}
	
	public int getColumnCount() {
	    return 3;
	}
	
	public Object getValueAt(int row, int column) {
	    switch (column) {
	    case 0:
		if (showAddresses) {
		    return recs[row].getClientIP();
		} else {
		    return recs[row].getClientName();
		}
	    case 1:
		return recs[row].getClientId();
	    case 2:
		return recs[row].getExpiration();
	    default:
		return null;
	    }
	}
	    
	public Class getColumnClass(int column) {
	    switch (column) {
	    case 0:
		if (showAddresses) {
		    return IPAddress.class;
		} else {
		    return String.class;
		}
	    case 1:
		return String.class;
	    case 2:
		return Date.class;
	    default:
		return Object.class;
	    }
	}

	public String getColumnName(int column) {
	    switch (column) {
	    case 0:
		if (showAddresses) {
		    return ResourceStrings.getString("address_column");
		} else {
		    return ResourceStrings.getString("client_name_column");
		}
	    case 1:
		return ResourceStrings.getString("client_column");
	    case 2:
		return ResourceStrings.getString("expires_column");
	    default:
		return null;
	    }
	}
    }    
    
    public ReleaseAddressDialog(Frame f, DhcpClientRecord [] clients,
	    String table, boolean showAddresses) {
	super(f, false);
	recs = clients;
	this.table = table;
	this.showAddresses = showAddresses;
    }
    
    public String getTitle() {
	return ResourceStrings.getString("release_address_title");
    }

    protected JPanel getMainPanel() {
	JPanel mainPanel = new JPanel(new BorderLayout(10, 10));
	mainPanel.setBorder(BorderFactory.createEmptyBorder(10, 10, 10, 10));
	
	JLabel message = new JLabel(
	    ResourceStrings.getString("release_address_confirm"));
	mainPanel.add(message, BorderLayout.NORTH);
	
	JTable addressTable = new JTable(new AddressTableModel());
	JScrollPane scrollPane = new JScrollPane(addressTable);
	Dimension d = addressTable.getPreferredScrollableViewportSize();
	d.height = 100;
	addressTable.setPreferredScrollableViewportSize(d);
	ExtendedCellRenderer renderer = new ExtendedCellRenderer();
	addressTable.setDefaultRenderer(Date.class, renderer);
	addressTable.setDefaultRenderer(IPAddress.class, renderer);
	
	mainPanel.add(scrollPane, BorderLayout.CENTER);
	buttonPanel.setOkEnabled(true);
	return mainPanel;
    }

    protected String getProgressMessage() {
    	return ResourceStrings.getString("release_addr_progress");
    }

    protected int getProgressLength() {
        return recs.length;
    }

    protected String getErrorHeading() {
    	return ResourceStrings.getString("address_column");
    }

    protected Class getErrorClass() {
    	return IPAddress.class;
    }

    protected Thread getOperationThread() {
	return new Thread() {
	    public void run() {
		DhcpNetMgr server = DataManager.get().getDhcpNetMgr();
		for (int i = 0; i < recs.length; ++i) {
		    DhcpClientRecord client = (DhcpClientRecord)recs[i].clone();
		    Date emptyDate = new Date(0);
		    try {
			// Clear client id and lease date
			client.setClientId(DhcpClientRecord.DEFAULT_CLIENT_ID);
			client.setExpiration(emptyDate);
			server.modifyClient(recs[i], client, table);
			// Update progress meter
			updateProgress(i+1, recs[i].getClientIPAddress());
		    } catch (InterruptedException e) {
			// User asked to stop
			closeDialog();
			return;
		    } catch (Throwable e) {
			addError(recs[i].getClientIP(), e.getMessage());
		    }
		}
		// Errors occurred, display them now
		if (errorsOccurred()) {
		    displayErrors(
		        ResourceStrings.getString("release_address_error"));
		}
		closeDialog();
	    }
	};
    }
    
    protected String getHelpKey() {
    	return "release_addresses";
    }

    protected void fireActionPerformed() {
	fireActionPerformed(this, DialogActions.OK);
    }
}
