/*
 * @(#)MainFrame.java	1.4	99/08/05 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.ui;

import javax.swing.*;
import javax.swing.event.*;
import java.awt.*;
import java.awt.event.*;
import java.util.*;
import java.net.URL;

/**
 * A main window container for an application which glues together multiple sets
 * of functionality (called Views) into a whole with a single menu bar,
 * status bar, and search functionality.
 * @see View
 */
public class MainFrame extends JFrame {

    class StatusBar extends JPanel {
	private JLabel data;
	private FindPanel finder;
	
	public StatusBar() {
	    super(new BorderLayout());
	    setBorder(BorderFactory.createEmptyBorder(0, 5, 0, 0));
	    data = new JLabel("", SwingConstants.LEFT);
	    Font f = data.getFont();
	    Font f2 = new Font(f.getName(), Font.PLAIN, f.getSize());
	    data.setFont(f2);
	    data.setForeground(Color.black);
	    add(data, BorderLayout.WEST);
	    finder = new FindPanel();
	    add(finder, BorderLayout.EAST);
	}
	
	public void setText(String s) {
	    data.setText(s);
	    // Force a relayout to avoid truncating text if longer
	    invalidate();
	    validate();
	}
	
	public String getText() {
	    return data.getText();
	}
    }
    
    /**
     * The panel with the Find control
     */
    class FindPanel extends JPanel implements ActionListener {
	private JTextField text;
	private JButton button;
	
	public void actionPerformed(ActionEvent e) {
	    activeView.find(text.getText());
	}
	
	public FindPanel() {
	    text = new JTextField("", 20);
	    button = new JButton(ResourceStrings.getString("next_button"));
	    button.addActionListener(this);
	    text.addActionListener(this);
    	    add(new JLabel(ResourceStrings.getString("find_label")));
	    add(text);
	    add(button);
	}
    }

    // Handler for the File->Exit menu item
    class ExitAction extends AbstractAction {
	public ExitAction() {
	    super(ResourceStrings.getString("exit_item"));
	}
	
	public void actionPerformed(ActionEvent e) {
	    setVisible(false);
	    dispose();
	}
    }
    
    // Handler for Edit->Create menu item
    class CreateAction extends AbstractAction {
	public CreateAction() {
	    super(ResourceStrings.getString("create_item"));
	}
	
	public void actionPerformed(ActionEvent e) {
	    activeView.handleCreate();
	}
    }
    
    // Handler for the Edit->Delete menu item
    class DeleteAction extends AbstractAction {
	public DeleteAction() {
	    super(ResourceStrings.getString("delete_item"));
	}
	
	public void actionPerformed(ActionEvent e) {
	    activeView.handleDelete();
	}
    }
    
    // Handler for the Edit->Duplicate menu item
    class DuplicateAction extends AbstractAction {
	public DuplicateAction() {
	    super(ResourceStrings.getString("duplicate_item"));
	}
	
	public void actionPerformed(ActionEvent e) {
	    activeView.handleDuplicate();
	}
    }
    
    // Handler for the Edit->Properties menu item
    class PropertiesAction extends AbstractAction {
	public PropertiesAction() {
	    super(ResourceStrings.getString("properties_item"));
	}
	
	public void actionPerformed(ActionEvent e) {
	    activeView.handleProperties();
	}
    }

    // Handler for the View->Update menu item
    class UpdateAction extends AbstractAction {
	public UpdateAction() {
	    super(ResourceStrings.getString("update_item"));
	}
	
	public void actionPerformed(ActionEvent e) {
	    activeView.handleUpdate();
	}
    }
    
    // Listener for selection events from the views
    class ViewSelectionListener implements SelectionListener {
	public void valueChanged() {
	    // Set menu item state on edit menu
	    for (int i = 1; i < menuActions[EDIT_MENU].length; ++i) {
		if (i == 2) {
		    // Duplicate can only be active for a single selection
		    menuActions[EDIT_MENU][i].setEnabled(
			!activeView.isSelectionEmpty()
			&& !activeView.isSelectionMultiple());
		} else {
		    menuActions[EDIT_MENU][i].setEnabled(
			!activeView.isSelectionEmpty());
		}
	    }
	}
    }	
    
    public static final int FILE_MENU = 0;
    public static final int EDIT_MENU = 1;
    public static final int VIEW_MENU = 2;
    public static final int ACTIONS_MENU = 3;
    public static final int HELP_MENU = 4;
    public static final int MENU_COUNT = 5;
    // The set of menus for the application
    private static String menuKeys[] = { "file_menu", "edit_menu", "view_menu",
	"actions_menu", "help_menu" };
    // The set of menu keyboard mnemonics
    private static int menuMnemonics[] = { KeyEvent.VK_F,
        KeyEvent.VK_E, KeyEvent.VK_V, KeyEvent.VK_S, KeyEvent.VK_H };
    /*
     *  Table of all the menu actions owned by MainFrame; some end up
     * delegating to the View.
     */
    private Action [] [] menuActions = { 
	// File Menu
	{ new ExitAction() },

	// Edit Menu
	{ new CreateAction(), new DeleteAction(), new DuplicateAction(),
	    new PropertiesAction() },

	// View Menu
	{ new UpdateAction() },

	// Actions Menu
	{ },

	// Help Menu
	{ }
    };
    private JMenuBar menuBar;
    private JMenu menuList[];
    private static StatusBar statusBar;
    private Component display = null;
    private JTabbedPane displayPanel;
    private View activeView, initialView;
    private Vector views;
    private int [] separatorIndex = new int[MENU_COUNT];
    private ButtonGroup viewButtonGroup = new ButtonGroup();
    private ViewSelectionListener viewSelectionListener =
	new ViewSelectionListener();
    private boolean initialized = false;

    public MainFrame() {
	this("");
    }
    
    public MainFrame(String title) {
	super(title);
	views = new Vector();
	Container contentPane = getContentPane();
	// Create basic menu structure
	menuBar = new JMenuBar();
	menuList = new JMenu[menuKeys.length];
	// First the menus
	for (int i = 0; i < menuList.length; ++i) {
	    menuList[i] = (JMenu)menuBar.add(
		new JMenu(ResourceStrings.getString(menuKeys[i])));
	    menuList[i].setMnemonic(menuMnemonics[i]);
	}
	// Now the items on each menu
	for (int i = 0; i < menuActions.length; ++i) {
	    for (int j = 0; j < menuActions[i].length; ++j) {
		menuList[i].add(menuActions[i][j]);
	    }
	}
	
	// separatorIndex will remember where we automatically put separators
	for (int i = 0; i < MENU_COUNT; ++i) {
	    separatorIndex[i] = -1;
	}
	
	setJMenuBar(menuBar);
	contentPane.setLayout(new BorderLayout());
	
	// Status bar for messages
	statusBar = new StatusBar();
	contentPane.add(statusBar, BorderLayout.SOUTH);
	
	// Display panel is the area for the view's main display
	displayPanel = new JTabbedPane();
	displayPanel.addChangeListener(new ChangeListener() {
	    public void stateChanged(ChangeEvent e) {
		/*
		 * Prevent premature activation of a view which would otherwise
		 * happen as a byproduct of adding views to the tabbed pane.
		 */
		if (initialized) {
		    try {
		    	setActiveView((View)views.elementAt(
			    displayPanel.getSelectedIndex()));
		    } catch (Throwable t) {
		   	t.printStackTrace();
		    }
		}
	    }
	});
	contentPane.add(displayPanel, BorderLayout.CENTER);
	activeView = null;
    }
    
    public static String getStatusText() {
	return statusBar.getText();
    }
    
    public static void setStatusText(String text) {
	statusBar.setText(text);
    }

    // Add a global menu
    public void addMenuAction(int menu, Action action) {
	menuList[menu].add(action);
    }
    
    // Add a view to the system
    public void addView(View v, boolean isInitial) {
	views.addElement(v);
	displayPanel.addTab(v.getName(), v.getDisplay());
	if (isInitial) {
	    initialView = v;
	}
	
	/*
	 * Listen to selection events from the view, update menu state
	 * accordingly
	 */
	v.addSelectionListener(viewSelectionListener);
    }
    
    // Delete a view
    public void deleteView(View v) {
	views.removeElement(v);
	/*
	 * If we're deleting the currently active view, need to activate
	 * another one; default to the initial view, unless that is also
	 * the one we're deleting, in which case just pick the first view.
	 */
	if (v == activeView) {
	    if (v != initialView) {
	    	setActiveView(initialView);
	    } else {
	        setActiveView((View)views.firstElement());
	    }
	}
	displayPanel.remove(v.getDisplay());
	v.removeSelectionListener(viewSelectionListener);
    }    
    
    // Select the view to be shown
    public void setActiveView(View v) {
	if (activeView != null) {
	    activeView.setActive(false);
	}
	// Remove custom menus from existing active view
        for (int i = MENU_COUNT + 1; i < menuBar.getMenuCount(); ++i) {
	    menuBar.remove(i);
	}
	/*
	 * Remove menu items on standard menus from existing active view,
	 * add those from new view
	 */
	for (int i = 0; i < menuList.length; ++i) {
	    JMenu m = menuBar.getMenu(i);
	    if (activeView != null) {
		Enumeration e = activeView.menuItems(i);
		if (e != null) {
		    if (separatorIndex[i] != -1) {
			m.remove(separatorIndex[i]);
			separatorIndex[i] = -1;
		    }
		    while (e.hasMoreElements()) {
			JMenuItem mi = (JMenuItem)e.nextElement();
			if (mi != null) {
			    m.remove((Component)mi);
			}
		    }
		}
	    }
	    Enumeration e = v.menuItems(i);
	    if (e != null) {
		while (e.hasMoreElements()) {
		    /*
		     * This test here so separator is only added if we
		     * actually get a menu item from the view, protecting
		     * against an empty enumeration causing a stray separator.
		     */
		    if (separatorIndex[i] == -1) {
			separatorIndex[i] = m.getItemCount();
			m.addSeparator();
		    }
		    JMenuItem mi = (JMenuItem)e.nextElement();
		    if (mi != null) {
			m.add(mi);
		    }
		}
	    }
	}
	
	// Add view's menus
	Enumeration e = v.menus();
	while ((e != null) && e.hasMoreElements()) {
	    JMenu m = (JMenu)e.nextElement();
	    if (m != null) {
		menuBar.add(m);
	    }
	}
	activeView = v;
	activeView.setActive(true);
	viewSelectionListener.valueChanged();
	invalidate();
	validate();
    }
    
    // Call this to get things started
    public void initialize() {
	if (initialView != null) {
	    setActiveView(initialView);
	    initialized = true;
	}
    }
    
    // Set enabled/disabled state for menus
    public void setMenuEnabled(int menu, boolean state) {
	menuList[menu].setEnabled(state);
    }
}
