/*
 * @(#)SelectOptionDialog.java	1.4	99/08/05 SMI
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
import java.text.MessageFormat;
import java.util.*;
import java.beans.PropertyChangeListener;
import java.beans.PropertyChangeEvent;

import com.sun.dhcpmgr.server.*;
import com.sun.dhcpmgr.data.*;
import com.sun.dhcpmgr.ui.*;

/**
 * Dialog to select an option for inclusion in a macro.
 */
public class SelectOptionDialog extends JComponent
	implements ButtonPanelListener {
    private JComboBox category;
    private AutosizingTable optionTable;
    private ButtonPanel buttonPanel;
    private OptionTableModel optionTableModel;
    private TableSorter sortedModel;
    static final String SELECTED_OPTION = "selected_option";
    static String value = null;
    static JDialog dialog;
    private static final String [] categories = {
	ResourceStrings.getString("option_contexts0"),
	ResourceStrings.getString("option_contexts1"),
	ResourceStrings.getString("option_contexts2"),
	ResourceStrings.getString("option_contexts3")
    };

    // Model for the table displaying option descriptions
    class OptionTableModel extends AbstractTableModel {
	private Option [] data;
	private ResourceBundle bundle;
	
	public OptionTableModel() {
	    super();
	    data = new Option[0];
	    // Locate the resource bundle containing the localized descriptions
	    bundle = ResourceBundle.getBundle(
		"com.sun.dhcpmgr.client.OptionDescriptions",
		Locale.getDefault());
	}
	
	public void setCategory(int category) {
	    switch (category) {
	    case Option.STANDARD:
		data = StandardOptions.getAllOptions();
		break;
	    case Option.EXTEND:
	    case Option.SITE:
	    case Option.VENDOR:
		try {
		    // Get all locally defined options from DataManager
		    Option [] allOptions = DataManager.get().getOptions(false);
		    Vector v = new Vector();
		    // Now filter by the selected type
		    for (int i = 0; i < allOptions.length; ++i) {
			if (allOptions[i].getContext() == category) {
			    v.addElement(allOptions[i]);
			}
		    }
		    // Convert to an array
		    data = new Option[v.size()];
		    v.copyInto(data);
		} catch (Exception e) {
		    data = new Option[0];
		}
		break;
	    }
	    // Tell the sorter things changed
	    sortedModel.reallocateIndexes();
	    fireTableDataChanged();
	}
	
	public int getRowCount() {
	    return data.length;
	}
	
	public int getColumnCount() {
	    return 2;
	}
	
	public Object getValueAt(int row, int column) {
	    if (column == 0) {
		return data[row].getKey();
	    } else {
		try {
		    /**
		     * Look up descriptions in the properties file indexed by
		     * option name
		     */
		    return bundle.getString(data[row].getKey());
		} catch (Exception e) {
		    // Ignore; we just don't have a description for this one
		    return null;

		}
	    }
	}
	
	public Class getColumnClass(int column) {
	    return String.class;
	}
	
	public String getColumnName(int column) {
	    if (column == 0) {
		return ResourceStrings.getString("option_column");
	    } else {
		return ResourceStrings.getString("description_column");
	    }
	}
	
	public boolean isCellEditable(int row, int column) {
	    return false;
	}
    }
    
    // Generate the dialog
    public void createDialog() {
	dialog = new JDialog((JFrame)null,
	    ResourceStrings.getString("select_option_title"), true);
	
	dialog.getContentPane().setLayout(new BoxLayout(dialog.getContentPane(),
	    BoxLayout.Y_AXIS));

	// Label and combo box for selecting option category
	JPanel panel = new JPanel(new FlowLayout(FlowLayout.LEFT));
	panel.add(new JLabel(ResourceStrings.getString("category_label")));

	category = new JComboBox(categories);
	category.setEditable(false);
	panel.add(category);
	
	dialog.getContentPane().add(panel);

	// Table for selecting the options in the given category	
	optionTableModel = new OptionTableModel();
	// Sort options by name, alphabetically
	sortedModel = new TableSorter(optionTableModel);
	sortedModel.sortByColumn(0);
	// Use an auto-sizing table so descriptions get the space they need
	optionTable = new AutosizingTable(sortedModel);
	optionTable.getTableHeader().setReorderingAllowed(false);
	optionTable.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);
	JScrollPane scrollPane = new JScrollPane(optionTable);
	panel = new JPanel();
	panel.add(scrollPane);
	dialog.getContentPane().add(panel);
	
	// Put in usual separator and buttons
	dialog.getContentPane().add(new JSeparator());
	buttonPanel = new ButtonPanel(false, false);
	buttonPanel.addButtonPanelListener(this);
	dialog.getContentPane().add(buttonPanel);
	
	/*
	 * As user changes category selected, update table to view category
	 * contents
	 */
	category.addItemListener(new ItemListener() {
	    public void itemStateChanged(ItemEvent e) {
		optionTableModel.setCategory(category.getSelectedIndex());
		optionTable.clearSelection();
	    }
	});
	
	// Only enable OK when there is an option selected in the table
	optionTable.getSelectionModel().addListSelectionListener(
		new ListSelectionListener() {
	    public void valueChanged(ListSelectionEvent e) {
		if (optionTable.getSelectedRow() == -1) {
		    buttonPanel.setOkEnabled(false);
		} else {
		    buttonPanel.setOkEnabled(true);
		}
	    }
	});
	
	// Default to the standard option set
	category.setSelectedIndex(Option.STANDARD);
    }
    
    public void buttonPressed(int buttonId) {
	switch (buttonId) {
	case OK:
	    firePropertyChange(SELECTED_OPTION, null,
		(String)optionTableModel.getValueAt(
		sortedModel.mapRowAt(optionTable.getSelectedRow()), 0));
	    break;
	case CANCEL:
	    firePropertyChange(SELECTED_OPTION, null, null);
	    break;
	}
    }
    
    /**
     * Here's the way to display this dialog modally and retrieve the value
     * selected
     * @param c a component relative to which the dialog should be displayed
     */
    public static String showDialog(Component c) {	
	SelectOptionDialog d = new SelectOptionDialog();
	d.createDialog();
	/*
	 * When user presses OK or Cancel, retrieve the value and kill the
	 * dialog
	 */
	d.addPropertyChangeListener(new PropertyChangeListener() {
	    public void propertyChange(PropertyChangeEvent e) {
		dialog.setVisible(false);
		dialog.dispose();
		value = (String)e.getNewValue();
	    }
	});
	dialog.setLocationRelativeTo(c);
	dialog.pack();
	dialog.show();
	return value;
    }
}
