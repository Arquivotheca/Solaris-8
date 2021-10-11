/*
 * @(#)OptionView.java	1.5	99/05/05 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.client;

import javax.swing.*;
import javax.swing.event.*;
import javax.swing.table.*;
import java.awt.*;
import java.awt.event.*;
import java.util.*;
import java.text.*;

import com.sun.dhcpmgr.ui.*;
import com.sun.dhcpmgr.server.*;
import com.sun.dhcpmgr.data.*;
import com.sun.dhcpmgr.bridge.BridgeException;

/**
 * The view for options.  Only displays locally defined options.
 */
public class OptionView implements View {

    // Model for the table displaying the options.
    class OptionTableModel extends AbstractTableModel {
	private Option [] data = null;
	private String [] ctxts;
	private String [] types;
	private boolean firstLoad;
    
	public OptionTableModel() {
	    ctxts = new String[Option.CONTEXTS];
	    for (int i = 0; i < ctxts.length; ++i) {
		ctxts[i] = ResourceStrings.getString("option_contexts"
		    + String.valueOf(i));
	    }
	    types = new String[Option.TYPES];
	    for (int i = 0; i < types.length; ++i) {
		types[i] = ResourceStrings.getString("option_types"
		    + String.valueOf(i));
	    }
	    firstLoad = true;
	}
	
	public void load() {
	    data = null;
	    // Update status line
	    MainFrame.setStatusText(
		ResourceStrings.getString("loading_options"));
	    fireTableDataChanged();

	    // Kick off background loading
	    OptionLoader loader = new OptionLoader();
	}
	
	protected void doneLoading() {
	    sortedTableModel.reallocateIndexes();
	    if (firstLoad) {
		sortedTableModel.sortByColumn(0);
		firstLoad = false;
	    }
	    // Check for any ill-defined options, tell user about them
	    Vector errs = new Vector();
	    for (int i = 0; i < data.length; ++i) {
	    	if (!data[i].isValid()) {
		    errs.addElement(data[i].getKey());
		}
	    }
	    if (errs.size() != 0) {
	        Object [] objs = new Object[2];
		objs[0] =
		    ResourceStrings.getString("option_validation_warning");
		JList optionList = new JList(errs);
		JScrollPane scrollPane = new JScrollPane(optionList);
		optionList.setVisibleRowCount(4);
		objs[1] = scrollPane;
		JOptionPane.showMessageDialog(optionTable, objs,
		    ResourceStrings.getString("warning"),
		    JOptionPane.WARNING_MESSAGE);
	    }
	    fireTableDataChanged();
	}
	
	protected void setData(Option [] newdata) {
	    data = newdata;
	}
	
	public int getRowCount() {
	    if (data == null) {
		return 0;
	    } else {
		return data.length;
	    }
	}
	
	public int getColumnCount() {
	    return 6;
	}
	
	public Object getValueAt(int row, int column) {	
	    switch (column) {
	    case 0:
		return data[row].getKey();
	    case 1:
		return ctxts[data[row].getContext()];
	    case 2:
		return new Integer(data[row].getCode());
	    case 3:
		return types[data[row].getType()];
	    case 4:
		return new Integer(data[row].getGranularity());
	    case 5:
		return new Integer(data[row].getMaximum());
	    default:
		return null;
	    }
	}
	
	public Class getColumnClass(int column) {
	    switch (column) {
	    case 0:
	    case 1:
	    case 3:
		return String.class;
	    case 2:
	    case 4:
	    case 5:
		return Integer.class;
	    default:
		super.getColumnClass(column);
	    }
	    return null;
	}
	
	public String getColumnName(int column) {
	    switch (column) {
	    case 0:
		return ResourceStrings.getString("name_column");
	    case 1:
		return ResourceStrings.getString("category_column");
	    case 2:
		return ResourceStrings.getString("code_column");
	    case 3:
		return ResourceStrings.getString("type_column");
	    case 4:
		return ResourceStrings.getString("granularity_column");
	    case 5:
		return ResourceStrings.getString("maximum_column");
	    default:
		super.getColumnName(column);
	    }
	    return null;
	}
	
	public Option getOptionAt(int row) {
	    return data[row];
	}
    }
    
    // Background loader for options.
    class OptionLoader extends SwingWorker {
	public Object construct() {
	    try {
		return DataManager.get().getOptions(true);
	    } catch (final BridgeException e) {
	    	// Since we're in a background thread, ask Swing to run ASAP.
	    	SwingUtilities.invokeLater(new Runnable() {
		    Object [] args = new Object[] { e.getMessage() };
	    	    public void run() {
		    	MessageFormat form = new MessageFormat(
		            ResourceStrings.getString("error_loading_options"));
		    	JOptionPane.showMessageDialog(null, form.format(args),
		            ResourceStrings.getString("server_error_title"),
			    JOptionPane.ERROR_MESSAGE);
		    }
	    	});
	    	return null;
	    }
	}
	
	public void finished() {
	    OptionView.optionTableModel.setData((Option [])get());
	    OptionView.optionTableModel.doneLoading();
	    Integer [] ints = new Integer[1];
	    ints[0] = new Integer(OptionView.optionTableModel.getRowCount());
	    MainFrame.setStatusText(MessageFormat.format(
		ResourceStrings.getString("option_status_message"), ints));
	    optionTable.clearSelection();
	}
    }

    /*
     * Reload data when any dialogs this view launches indicate they've
     * changed the data
     */
    class DialogListener implements ActionListener {
	public void actionPerformed(ActionEvent e) {
	    if (!e.getActionCommand().equals(DialogActions.CANCEL)) {
	        reload();
	    }
	}
    }

    private AutosizingTable optionTable;
    private JScrollPane optionPane;
    protected static OptionTableModel optionTableModel = null;
    private JCheckBoxMenuItem showGrid;
    private JMenuItem optionHelp;
    private Vector[] menuItems;
    private Frame myFrame;
    private static boolean firstview = true;
    private Vector selectionListeners = new Vector();
    private TableSorter sortedTableModel;
    
    public OptionView() {
	// Create table to display in data area
	optionTableModel = new OptionTableModel();
	sortedTableModel = new TableSorter(optionTableModel);

	// Use table which resizes columns to fit data
	optionTable = new AutosizingTable(sortedTableModel);
	sortedTableModel.addMouseListenerToHeaderInTable(optionTable);
	optionTable.getTableHeader().setReorderingAllowed(true);
	optionTable.getTableHeader().setResizingAllowed(true);
	optionTable.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);	

	// Allow clicks in header to adjust sorting column
	sortedTableModel.addActionListener(new ActionListener() {
    	    private int sortModelIndex = -1;
    	    private TableCellRenderer savedRenderer;
    	    private SortedHeaderRenderer sortedRenderer =
	        new SortedHeaderRenderer(optionTable);
	    
	    public void actionPerformed(ActionEvent e) {
		// Clear the selection if sorting is changed
		optionTable.clearSelection();

		// Change the header rendering to show sorting column
		int modelIndex = Integer.parseInt(e.getActionCommand());
		int viewIndex =
		    optionTable.convertColumnIndexToView(modelIndex);
		if (sortModelIndex != -1) {
		    int sortViewIndex =
		        optionTable.convertColumnIndexToView(sortModelIndex);
		    optionTable.getColumnModel().getColumn(sortViewIndex).
			setHeaderRenderer(savedRenderer);
		}
		/*
		 * Save the column currently being sorted so we can restore
		 * the renderer later.  We save model columns rather than
		 * view columns because model columns are invariant while
		 * view columns can be reordered with confusion resulting.
		 */
		TableColumn c =
		    optionTable.getColumnModel().getColumn(viewIndex);
		savedRenderer = c.getHeaderRenderer();
		c.setHeaderRenderer(sortedRenderer);
		sortModelIndex = modelIndex;	
	    }
	});
	
	// Make it scrollable
	optionPane = new JScrollPane(optionTable);

	// Make double-clicks the same as Edit->Properties
	optionTable.addMouseListener(new MouseAdapter() {
	    public void mouseClicked(MouseEvent e) {
		if (e.getClickCount() == 2) {
			handleProperties();
		}
	    }
	});

	// Create menu items
	showGrid = new JCheckBoxMenuItem(ResourceStrings.getString("show_grid"),
	    true);
	showGrid.addActionListener(new ActionListener() {
	    public void actionPerformed(ActionEvent e) {
		optionTable.setShowGrid(showGrid.getState());
	    }
	});

	optionHelp = new JMenuItem(
	    ResourceStrings.getString("on_options_item"));
	optionHelp.addActionListener(new ActionListener() {
	    public void actionPerformed(ActionEvent e) {
		DhcpmgrApplet.showHelp("options_reference");
	    }
	});
	
	/*
	 * Build the sets of menu items which we'll return to 
	 * MainFrame when it asks for them.
	 */
	menuItems = new Vector[MainFrame.MENU_COUNT];
	for (int i = 0; i < menuItems.length; ++i) {
	    menuItems[i] = new Vector();
	}
	menuItems[MainFrame.VIEW_MENU].addElement(showGrid);
	menuItems[MainFrame.HELP_MENU].addElement(optionHelp);

	// Listen for selections events, manipulate menu item state as needed
	optionTable.getSelectionModel().addListSelectionListener(
		new ListSelectionListener() {
	    public void valueChanged(ListSelectionEvent e) {
		// Notify listeners that our selection state may have changed
		notifySelectionListeners();
	    }
	});
    }
    
    public String getName() {
	return ResourceStrings.getString("option_view_name");
    }
    
    // Return custom menus we want added, in our case none.
    public Enumeration menus() {
	return null;
    }
    
    // Return menu items for each menu as requested by MainFrame
    public Enumeration menuItems(int menu) {
	return menuItems[menu].elements();
    }
    
    public Component getDisplay() {
	return optionPane;
    }
    
    public void setActive(boolean state) {
	if (state) {
	    // Things we do only the first time we're displayed
	    if (firstview) {
		myFrame = (Frame)SwingUtilities.getAncestorOfClass(
		    MainFrame.class, optionTable);
		optionTableModel.load();
		Integer [] ints = {new Integer(sortedTableModel.getRowCount())};
		String s = MessageFormat.format(
		    ResourceStrings.getString("option_status_message"), ints);
		MainFrame.setStatusText(s);
		firstview = false;
	    }
	}
    }
    
    public void find(String s) {
	int startRow = optionTable.getSelectedRow() + 1;
	for (int i = startRow; i < sortedTableModel.getRowCount(); ++i) {
	    if (optionTableModel.getOptionAt(
		    sortedTableModel.mapRowAt(i)).toString().indexOf(s) != -1) {
		optionTable.setRowSelectionInterval(i, i);
		optionTable.scrollRectToVisible(optionTable.getCellRect(i, 0,
		    false));
		return;
	    }
	}
	// Got to the end, wrap around
	for (int i = 0; i < startRow; ++i) {
	    if (optionTableModel.getOptionAt(
		    sortedTableModel.mapRowAt(i)).toString().indexOf(s) != -1) {
		optionTable.setRowSelectionInterval(i, i);
		optionTable.scrollRectToVisible(optionTable.getCellRect(i, 0,
		    false));
		return;
	    }
	}   
    }
    
    public void handleCreate() {
	CreateOptionDialog d = new CreateOptionDialog(myFrame,
	    CreateOptionDialog.CREATE);
	d.addActionListener(new DialogListener());
	d.pack();
	d.show();
    }
    
    public void handleDelete() {
	int selectedRow = optionTable.getSelectedRow();
	if (selectedRow == -1) {
	    return;
	}
	DeleteOptionDialog d = new DeleteOptionDialog(myFrame,
	    optionTableModel.getOptionAt(
	    sortedTableModel.mapRowAt(selectedRow)));
	d.addActionListener(new DialogListener());
	d.pack();
	d.show();
    }
    
    public void handleDuplicate() {
	int selectedRow = optionTable.getSelectedRow();
	if (selectedRow == -1) {
	    return;
	}
	CreateOptionDialog d = new CreateOptionDialog(myFrame,
	    CreateOptionDialog.DUPLICATE);
	d.setOption(optionTableModel.getOptionAt(
	    sortedTableModel.mapRowAt(selectedRow)));
	d.addActionListener(new DialogListener());
	d.pack();
	d.show();
    }
    
    public void handleProperties() {
	int selectedRow = optionTable.getSelectedRow();
	if (selectedRow == -1) {
	    return;
	}
	CreateOptionDialog d = new CreateOptionDialog(myFrame,
	    CreateOptionDialog.EDIT);
	d.setOption(optionTableModel.getOptionAt(
	    sortedTableModel.mapRowAt(selectedRow)));
	d.addActionListener(new DialogListener());
	d.pack();
	d.show();
    }
    
    public void handleUpdate() {
	reload();
    }
    
    private void reload() {
	optionTableModel.load();
    }

    public void addSelectionListener(SelectionListener listener) {
	selectionListeners.addElement(listener);
    }
    
    public void removeSelectionListener(SelectionListener listener) {
	selectionListeners.removeElement(listener);
    }
    
    private void notifySelectionListeners() {
	Enumeration en = selectionListeners.elements();
	while (en.hasMoreElements()) {
	    SelectionListener l = (SelectionListener)en.nextElement();
	    l.valueChanged();
	}
    }
    
    public boolean isSelectionEmpty() {
	return optionTable.getSelectionModel().isSelectionEmpty();
    }
    
    public boolean isSelectionMultiple() {
	return false; // Multiple selection is not allowed.
    }
}
