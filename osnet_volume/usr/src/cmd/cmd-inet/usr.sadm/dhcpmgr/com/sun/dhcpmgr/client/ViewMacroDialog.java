/*
 * @(#)ViewMacroDialog.java	1.4	99/08/26 SMI
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
import java.util.*;

import com.sun.dhcpmgr.data.*;
import com.sun.dhcpmgr.ui.*;

/**
 * This dialog allows the user to view the contents of a macro,
 */
public class ViewMacroDialog extends JDialog {

    class MacroTableModel extends AbstractTableModel {
	private Macro macro;
	
	public MacroTableModel() {
	    setMacro(new Macro());
	}
	
	public MacroTableModel(Macro m) {
	    super();
	    setMacro(m);
	}
	
	public void setMacro(Macro m) {
	    macro = m;
	    fireTableDataChanged();
	}
	
	public int getRowCount() {
	    return macro.optionCount();
	}
	
	public int getColumnCount() {
	    return 2;
	}
	
	public Object getValueAt(int row, int column) {
	    OptionValue v = null;	
	    try {
		v = macro.getOptionAt(row);
	    } catch (ArrayIndexOutOfBoundsException e) {
		return null;
	    }
	    if (v == null) {
		return null;
	    }
	    switch (column) {
	    case 0:
		return v.getName();
	    case 1:
		return v.getValue();
	    default:
		return null;
	    }
	}
	
	public Class getColumnClass(int column) {
	    switch (column) {
	    case 0:
	    case 1:
		return String.class;
	    default:
		super.getColumnClass(column);
	    }
	    return null;
	}
	
	public String getColumnName(int column) {
	    switch (column) {
	    case 0:
		return ResourceStrings.getString("option_column");
	    case 1:
		return ResourceStrings.getString("value_column");
	    default:
		super.getColumnName(column);
	    }
	    return null;
	}
    }
    
    private JTextField name;
    private AutosizingTable macroTable;
    private MacroTableModel macroTableModel;
    private JButton closeButton;
    
    /**
     * Construct the dialog.
     * @arg owner The owning dialog
     * @arg c The component relative to which we should be positioned
     * @arg macro The macro we're viewing
     */
    public ViewMacroDialog(Dialog owner, Component c, Macro macro) {
	super(owner);
	setLocationRelativeTo(c);
	
	setTitle(ResourceStrings.getString("view_macro_title"));
	
	getContentPane().setLayout(new BoxLayout(getContentPane(),
	    BoxLayout.Y_AXIS));
	JPanel mainPanel = new JPanel(new BorderLayout());
	mainPanel.setBorder(BorderFactory.createEmptyBorder(10, 10, 10, 10));
	
	name = new JTextField(30);
	name.setEditable(false);
	JPanel panel = new JPanel();
	panel.add(new JLabel(ResourceStrings.getString("name_label")));
	panel.add(name);
	mainPanel.add(panel, BorderLayout.NORTH);
	
	JPanel contentsPanel = new JPanel();
	contentsPanel.setLayout(new BorderLayout());
	Border b = BorderFactory.createCompoundBorder(
	    BorderFactory.createLineBorder(Color.black),
	    BorderFactory.createEmptyBorder(5, 10, 5, 10));
	contentsPanel.setBorder(BorderFactory.createTitledBorder(b,
	    ResourceStrings.getString("contents_label")));
	macroTableModel = new MacroTableModel();
	macroTable = new AutosizingTable(macroTableModel);
	macroTable.getTableHeader().setReorderingAllowed(false);
	macroTable.getTableHeader().setResizingAllowed(false);

	JScrollPane macroTablePane = new JScrollPane(macroTable);
	// Resize table as otherwise it asks for a huge area
	Dimension d = macroTable.getPreferredScrollableViewportSize();
	d.height = 100;
	d.width = 300;
	macroTable.setPreferredScrollableViewportSize(d);
	
	contentsPanel.add(macroTablePane, BorderLayout.CENTER);
	mainPanel.add(contentsPanel, BorderLayout.CENTER);
		
	getContentPane().add(mainPanel);
	getContentPane().add(new JSeparator());
	
	JPanel buttonPanel = new JPanel();
	closeButton = new JButton(ResourceStrings.getString("ok"));
	buttonPanel.add(closeButton);
	closeButton.addActionListener(new ActionListener() {
	    public void actionPerformed(ActionEvent e) {
		setVisible(false);
		dispose();
	    }
	});
	
	getContentPane().add(buttonPanel);
	
        name.setText(macro.getKey());
	macroTableModel.setMacro(macro);
    }
}
