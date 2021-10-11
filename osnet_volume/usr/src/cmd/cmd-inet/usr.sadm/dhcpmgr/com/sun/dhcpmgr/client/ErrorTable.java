/*
 * @(#)ErrorTable.java	1.1	99/05/05 SMI
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.client;

import javax.swing.table.AbstractTableModel;
import java.util.Vector;
import java.util.Date;

import com.sun.dhcpmgr.ui.AutosizingTable;
import com.sun.dhcpmgr.ui.ExtendedCellRenderer;
import com.sun.dhcpmgr.data.IPAddress;

/*
 * The model for the error table.
 */
class ErrorTableModel extends AbstractTableModel {
    private String column0Label;
    private Class column0Class;
    private Vector rows;

    public ErrorTableModel(String column0Label, Class column0Class) {
        this.column0Label = column0Label;
	this.column0Class = column0Class;
	rows = new Vector();
    }

    public int getRowCount() {
    	return rows.size();
    }

    public int getColumnCount() {
    	return 2;
    }

    public Object getValueAt(int row, int column) {
	Object [] ro = (Object [])rows.elementAt(row);
	return ro[column];
    }

    public Class getColumnClass(int column) {
        if (column == 0) {
	    return column0Class;
	} else {
	    return String.class;
	}
    }

    public String getColumnName(int column) {
        if (column == 0) {
	    return column0Label;
	} else {
	    return ResourceStrings.getString("error_message");
	}
    }

    public void addError(Object o, String msg) {
        Object [] row = new Object[] { o, msg };
    	rows.addElement(row);
    }
}

/**
 * A table for displaying errors which occurred while acting on multiple
 * objects.
 */
public class ErrorTable extends AutosizingTable {
    ErrorTableModel model;

    public ErrorTable(String column0Label, Class column0Class) {
    	super();
	model = new ErrorTableModel(column0Label, column0Class);
	setModel(model);
	ExtendedCellRenderer renderer = new ExtendedCellRenderer();
	setDefaultRenderer(Date.class, renderer);
	setDefaultRenderer(IPAddress.class, renderer);
    }

    public void addError(Object o, String msg) {
        model.addError(o, msg);
    }

    public boolean isEmpty() {
        return (model.getRowCount() == 0);
    }
}
