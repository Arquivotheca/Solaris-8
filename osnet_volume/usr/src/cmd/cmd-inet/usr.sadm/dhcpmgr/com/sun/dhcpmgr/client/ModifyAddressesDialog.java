/*
 * @(#)ModifyAddressesDialog.java	1.4	99/06/03 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.client;

import javax.swing.*;
import javax.swing.table.*;
import java.util.*;
import java.awt.*;
import java.awt.event.*;
import java.net.*;

import com.sun.dhcpmgr.data.*;
import com.sun.dhcpmgr.ui.*;
import com.sun.dhcpmgr.server.DhcpNetMgr;

/**
 * This dialog allows the user to modify multiple addresses
 */
public class ModifyAddressesDialog extends MultipleOperationDialog {
    private DhcpClientRecord [] recs;
    private String table;
    private JLabel numberLabel;
    private JComboBox server;
    private JTextField comment;
    private JComboBox macro;
    private JRadioButton bootpCurrent, bootpAll, bootpNone;
    private JRadioButton unusableCurrent, unusableAll, unusableNone;
    private JRadioButton leaseCurrent, leaseDynamic, leasePermanent;
    private static final String keepString =
        ResourceStrings.getString("modify_multiple_keep");
    
    class ServerListModel extends AbstractListModel implements ComboBoxModel {
	private String [] servers;
	private Object currentValue;
	
	public ServerListModel() {
	    servers = new String[2];
	    servers[0] = keepString;
	    servers[1] = DataManager.get().getShortServerName();
	}
	
	public int getSize() {
	    return servers.length;
	}
	
	public Object getElementAt(int index) {
	    return servers[index];
	}
	
	public void setSelectedItem(Object anItem) {
	    currentValue = anItem;
	    fireContentsChanged(this, -1, -1);
	}
	
	public Object getSelectedItem() {
	    return currentValue;
	}
    }
    
    class MacroListModel extends AbstractListModel implements ComboBoxModel {
	private String [] macros;
	private Object currentValue;
	
	public MacroListModel() {
	    Macro [] macs = new Macro[0];
	    try {
		macs = DataManager.get().getMacros(false);
		macros = new String[macs.length + 1];
	    } catch (Throwable e) {
		macros = new String[1];
	    }
	    macros[0] = keepString;
	    for (int i = 0; i < macs.length; ++i) {
		macros[i+1] = macs[i].getKey();
	    }
	}
	
	public int getSize() {
	    return macros.length;
	}
	
	public Object getElementAt(int index) {
	    return macros[index];
	}
	
	public void setSelectedItem(Object anItem) {
	    currentValue = anItem;
	    fireContentsChanged(this, -1, -1);
	}
	
	public Object getSelectedItem() {
	    return currentValue;
	}
    }
    
    public ModifyAddressesDialog(Frame f, DhcpClientRecord [] clients,
	    String table) {
	// Create dialog with reset button
	super(f, true);

	this.table = table;
	recs = clients;
	numberLabel.setText(String.valueOf(recs.length));
    }

    public String getTitle() {
	return ResourceStrings.getString("modify_multiple_addresses");
    }

    protected JPanel getMainPanel() {
	JPanel mainPanel = new JPanel();
	mainPanel.setBorder(BorderFactory.createEmptyBorder(10, 10, 10, 10));
	GridBagLayout bag = new GridBagLayout();
	mainPanel.setLayout(bag);
	GridBagConstraints con = new GridBagConstraints();
	con.gridx = con.gridy = 0;
	con.weightx = con.weighty = 1.0;
	con.insets = new Insets(5, 5, 5, 5);
	con.gridwidth = con.gridheight = 1;
	
	// Number of addresses
	JLabel label = new JLabel(
	    ResourceStrings.getString("modify_multiple_number"));
	con.anchor = GridBagConstraints.EAST;
	bag.setConstraints(label, con);
	mainPanel.add(label);

	numberLabel = new JLabel("100");
	numberLabel.setForeground(Color.black);
	++con.gridx;
	con.anchor = GridBagConstraints.WEST;
	bag.setConstraints(numberLabel, con);
	mainPanel.add(numberLabel);
	
	// Server
	label = new JLabel(ResourceStrings.getString("modify_multiple_server"));
	con.anchor = GridBagConstraints.EAST;
	con.gridx = 0;
	con.gridy += 2;
	bag.setConstraints(label, con);
	mainPanel.add(label);
	
	server = new JComboBox(new ServerListModel());
	server.setEditable(true);
	con.anchor = GridBagConstraints.WEST;
	++con.gridx;
	bag.setConstraints(server, con);
	mainPanel.add(server);
	
	// Comment
	label = new JLabel(
	    ResourceStrings.getString("modify_multiple_comment"));
	con.anchor = GridBagConstraints.EAST;
	con.gridx = 0;
	++con.gridy;
	bag.setConstraints(label, con);
	mainPanel.add(label);

	comment = new JTextField(20);
	con.anchor = GridBagConstraints.WEST;
	++con.gridx;
	bag.setConstraints(comment, con);
	mainPanel.add(comment);
	
	// Macro
	label = new JLabel(ResourceStrings.getString("modify_multiple_macro"));
	con.anchor = GridBagConstraints.EAST;
	con.gridx = 0;
	++con.gridy;
	bag.setConstraints(label, con);
	mainPanel.add(label);

	macro = new JComboBox(new MacroListModel());
	con.anchor = GridBagConstraints.WEST;
	++con.gridx;
	bag.setConstraints(macro, con);
	mainPanel.add(macro);
	
	// BootP
	label = new JLabel(ResourceStrings.getString("modify_multiple_bootp"));
	con.anchor = GridBagConstraints.EAST;
	con.gridx = 0;
	con.gridy += 2;
	bag.setConstraints(label, con);
	mainPanel.add(label);

	ButtonGroup bootpGroup = new ButtonGroup();
	bootpCurrent = new JRadioButton(keepString);
	bootpGroup.add(bootpCurrent);
	con.anchor = GridBagConstraints.WEST;
	++con.gridx;
	bag.setConstraints(bootpCurrent, con);
	mainPanel.add(bootpCurrent);
	
	bootpAll = new JRadioButton(
	    ResourceStrings.getString("modify_multiple_bootp_all"));
	bootpGroup.add(bootpAll);
	++con.gridy;
	bag.setConstraints(bootpAll, con);
	mainPanel.add(bootpAll);
	
	bootpNone = new JRadioButton(
	    ResourceStrings.getString("modify_multiple_bootp_none"));
	bootpGroup.add(bootpNone);
	++con.gridy;
	bag.setConstraints(bootpNone, con);
	mainPanel.add(bootpNone);
	
	// Unusable
	label = new JLabel(
	    ResourceStrings.getString("modify_multiple_unusable"));
	con.anchor = GridBagConstraints.EAST;
	con.gridx = 0;
	con.gridy += 2;
	bag.setConstraints(label, con);
	mainPanel.add(label);

	ButtonGroup unusableGroup = new ButtonGroup();
	unusableCurrent = new JRadioButton(keepString);
	unusableGroup.add(unusableCurrent);
	con.anchor = GridBagConstraints.WEST;
	++con.gridx;
	bag.setConstraints(unusableCurrent, con);
	mainPanel.add(unusableCurrent);
	
	unusableAll = new JRadioButton(
	    ResourceStrings.getString("modify_multiple_unusable_all"));
	unusableGroup.add(unusableAll);
	++con.gridy;
	bag.setConstraints(unusableAll, con);
	mainPanel.add(unusableAll);
	
	unusableNone = new JRadioButton(
	    ResourceStrings.getString("modify_multiple_unusable_none"));
	unusableGroup.add(unusableNone);
	++con.gridy;
	bag.setConstraints(unusableNone, con);
	mainPanel.add(unusableNone);
	
	// Lease
	label = new JLabel(ResourceStrings.getString("modify_multiple_lease"));
	con.anchor = GridBagConstraints.EAST;
	con.gridx = 0;
	con.gridy += 2;
	bag.setConstraints(label, con);
	mainPanel.add(label);

	ButtonGroup leaseGroup = new ButtonGroup();
	leaseCurrent = new JRadioButton(keepString);
	leaseGroup.add(leaseCurrent);
	con.anchor = GridBagConstraints.WEST;
	++con.gridx;
	bag.setConstraints(leaseCurrent, con);
	mainPanel.add(leaseCurrent);
	
	leaseDynamic = new JRadioButton(
	    ResourceStrings.getString("modify_multiple_dynamic"));
	leaseGroup.add(leaseDynamic);
	++con.gridy;
	bag.setConstraints(leaseDynamic, con);
	mainPanel.add(leaseDynamic);
	
	leasePermanent = new JRadioButton(
	    ResourceStrings.getString("modify_multiple_permanent"));
	leaseGroup.add(leasePermanent);
	++con.gridy;
	bag.setConstraints(leasePermanent, con);
	mainPanel.add(leasePermanent);
	
	buttonPanel.setOkEnabled(true);
	doReset();

	return mainPanel;
    }
    
    protected void doReset() {
	server.setSelectedIndex(0);
	comment.setText(keepString);
	macro.setSelectedIndex(0);
	bootpCurrent.setSelected(true);
	unusableCurrent.setSelected(true);
	leaseCurrent.setSelected(true);
    }
    
    protected String getProgressMessage() {
    	return ResourceStrings.getString("modify_multiple_progress");
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
		DhcpNetMgr netMgr = DataManager.get().getDhcpNetMgr();
		for (int i = 0; i < recs.length; ++i) {
		    DhcpClientRecord client = (DhcpClientRecord)recs[i].clone();
		    try {
			String s = (String)server.getSelectedItem();
			if (!s.equals(keepString)) {
			    // Change server if necessary
			    client.setServerIP(new IPAddress(s));
			}
			s = comment.getText();
			if (!s.equals(keepString)) {
			    client.setComment(s);
			}
			if (macro.getSelectedIndex() != 0) {
			    client.setMacro((String)macro.getSelectedItem());
			}
			if (bootpAll.isSelected()) {
			    client.setBootp(true);
			} else if (bootpNone.isSelected()) {
			    client.setBootp(false);
			}
			if (unusableAll.isSelected()) {
			    client.setUnusable(true);
			} else if (unusableNone.isSelected()) {
			    client.setUnusable(false);
			}
			if (leaseDynamic.isSelected()) {
			    client.setPermanent(false);
			} else if (leasePermanent.isSelected()) {
			    client.setPermanent(true);
			}
			netMgr.modifyClient(recs[i], client, table);
			updateProgress(i+1, client.getClientIPAddress());
		    } catch (InterruptedException e) {
		    	closeDialog();
			return;
		    } catch (Throwable e) {
			addError(recs[i].getClientIP(), e.getMessage());
		    }
		}
		// Errors occurred, display them
		if (errorsOccurred()) {
		    displayErrors(
		        ResourceStrings.getString("modify_multiple_error"));
	    	}
	    	closeDialog();
	    }
	};
    }
    
    protected String getHelpKey() {
	return "modify_multiple_addresses";
    }
    
    protected void fireActionPerformed() {
	fireActionPerformed(this, DialogActions.OK);
    }
}
