/*
 * @(#)SortedHeaderRenderer.java	1.1	99/03/22 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.ui;

import javax.swing.JTable;
import javax.swing.table.TableCellRenderer;
import java.awt.Font;
import java.awt.Component;

/**
 * This class provides the functionality to indicate to the user which column
 * a table is currently being sorted on.  At present it merely uses a bold
 * font to display the column name, and does not indicate the collation order.
 */
public class SortedHeaderRenderer implements TableCellRenderer {
    TableCellRenderer renderer;
    
    public SortedHeaderRenderer(JTable table) {
	renderer = table.getColumnModel().getColumn(0).getHeaderRenderer();
    }
    
    public Component getTableCellRendererComponent(JTable table, Object value,
	    boolean isSelected, boolean hasFocus, int row, int column) {
	Component c = renderer.getTableCellRendererComponent(table, value,
	    isSelected, hasFocus, row, column);
	Font f = c.getFont();
	c.setFont(new Font(f.getName(), Font.BOLD, f.getSize()));
	return c;
    }
}
