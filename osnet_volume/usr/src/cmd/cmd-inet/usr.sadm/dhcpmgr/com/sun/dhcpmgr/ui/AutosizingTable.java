/*
 * @(#)AutosizingTable.java	1.2	99/05/05 SMI
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.ui;

import javax.swing.JTable;
import javax.swing.table.TableModel;
import javax.swing.event.TableModelEvent;
import javax.swing.table.TableColumn;
import java.awt.Component;
import java.util.Date;

import com.sun.dhcpmgr.data.IPAddress;
import com.sun.dhcpmgr.data.ValidationException;

/**
 * A subclass of Swing's JTable which automatically resizes its columns to
 * fit the data being displayed in them.
 */ 
public class AutosizingTable extends JTable {
    Date aDate = null;
    IPAddress longIP = null;
    
    /**
     * Construct the table
     */
    public AutosizingTable() {
        super();
    }

    /**
     * Construct the table with the given model.
     * @param model the TableModel to be used.
     */
    public AutosizingTable(TableModel model) {
	super(model);
    }
    
    /**
     * The table has changed; we'll resize the columns to contain the new data
     * as best they can.
     */
    public void tableChanged(TableModelEvent e) {
	/*
	 * Let JTable do its thing, which probably includes wiping out 
	 * the old columns and creating new ones
	 */
	super.tableChanged(e);
	TableModel model = getModel();
	if (model.getRowCount() == 0) {
	    // No data, so just skip all the gymnastics
	    return;
	}
	/*
	 * Set column widths by first finding largest value in each column
	 * and then sizing accordingly
	 */
	for (int i = 0; i < getColumnCount(); ++i) {
	    TableColumn col = getColumnModel().getColumn(i);

	    // Get the width of the header for this column
	    Component c = col.getHeaderRenderer().getTableCellRendererComponent(
		null, col.getHeaderValue(), false, false, 0, 0);
	    int headerWidth = c.getPreferredSize().width;
	    Object maxVal = null;

	    if (model.getColumnClass(i) == String.class) {
		// Column contains strings; find the longest one
		String maxString = "";
		for (int j = 0; j < model.getRowCount(); ++j) {
		    String s = (String)model.getValueAt(j, i);
		    if (s != null) {
			if (maxString.length() < s.length()) {
			    maxString = s;
			}
		    }
		}
		maxVal = maxString;
	    } else if (model.getColumnClass(i) == IPAddress.class) {
		// Column contains IP addresses; one long one is as good as any
		if (longIP == null) {
		    try {
			longIP = new IPAddress("222.222.222.222");
		    } catch (ValidationException ex) {
			// This should never happen!
		    }
		}
		maxVal = longIP;
	    } else if (model.getColumnClass(i) == Date.class) {
		// Column contains dates; now is as good a time as any other.
		if (aDate == null) {
		    aDate = new Date();
		}
		maxVal = aDate;
	    }
	    // Now compute the width of the cell containing the longest value
	    c = getDefaultRenderer(
		model.getColumnClass(i)).getTableCellRendererComponent(
		this, maxVal, false, false, 0, i);
	    int cellWidth = c.getPreferredSize().width;

	    // Set preferred width to the greater of the header & cell widths
	    col.setPreferredWidth(Math.max(headerWidth, cellWidth));
	}
	// Now force the resizing we just did to be displayed
	sizeColumnsToFit(-1);

	// Force header to repaint itself, otherwise it won't align correctly
	getTableHeader().resizeAndRepaint();
    }
}
