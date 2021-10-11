/*
 * @(#)RelayTargetTable.java	1.1	99/03/22 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.client;

import javax.swing.*;
import javax.swing.table.*;
import javax.swing.event.*;
import java.util.*;
import java.text.*;

import com.sun.dhcpmgr.data.*;
import com.sun.dhcpmgr.ui.*;

/**
 * This class provides a tabular view of the target servers for a relay config
 */
public class RelayTargetTable extends JTable {
    class RelayTargetTableModel extends AbstractTableModel {
	Vector servers;
	
	public RelayTargetTableModel() {
	    servers = new Vector();
	}
	
	public void setServerList(String s) {
	    servers.removeAllElements();
	    StringTokenizer st = new StringTokenizer(s, ",");
	    while (st.hasMoreTokens()) {
		servers.addElement(st.nextToken());
	    }
	    fireTableDataChanged();
	}
	
	public String getServerList() {
	    StringBuffer b = new StringBuffer();
	    Enumeration en = servers.elements();
	    while (en.hasMoreElements()) {
		IPAddress a = (IPAddress)en.nextElement();
		if (b.length() != 0) {
		    b.append(',');
		}
		b.append(a.getHostAddress());
	    }
	    return b.toString();
	}
		
	public int getRowCount() {
	    return servers.size() + 1; // Extra row for inserting data
	}
	
	public int getColumnCount() {
	    return 1;
	}
	
	public Object getValueAt(int row, int column) {
	    if (row == 0) {
		return null;
	    } else {
		return servers.elementAt(row-1);
	    }
	}
	
	public void setValueAt(Object o, int row, int column) {
	    String s = (String)o;
	    if (s == null || s.length() == 0) {
		if (row != 0) {
		    servers.removeElementAt(row-1);
		    fireTableRowsDeleted(row, row);
		}
		return;
	    }
	    try {
		IPAddress addr = new IPAddress(s);
		if (row == 0) {
		    servers.insertElementAt(addr, 0);
		    fireTableRowsInserted(0, 1);
		} else {
		    servers.setElementAt(addr, row-1);
		}
	    } catch (ValidationException e) {
		Object [] args = new Object[1];
		MessageFormat form = new MessageFormat(
		    ResourceStrings.getString("configure_relay_lookup_error"));
		args[0] = o;
		JOptionPane.showMessageDialog(RelayTargetTable.this,
		    form.format(args),
		    ResourceStrings.getString("server_error_title"),
		    JOptionPane.ERROR_MESSAGE);
	    }
	}
	
	public Class getColumnClass(int column) {
	    return IPAddress.class;
	}
	
	public String getColumnName(int column) {
	    return ResourceStrings.getString("servers_column");
	}
	
	public boolean isCellEditable(int row, int column) {
	    return true;
	}
    }
    
    RelayTargetTableModel tableModel;
    
    public RelayTargetTable() {
	super();
	tableModel = new RelayTargetTableModel();
	setModel(tableModel);
	setSelectionMode(ListSelectionModel.SINGLE_SELECTION);
	setDefaultRenderer(IPAddress.class, new ExtendedCellRenderer());
    }
    
    public void setServerList(String s) {
	tableModel.setServerList(s);
    }
    
    public String getServerList() {
	return tableModel.getServerList();
    }
}
