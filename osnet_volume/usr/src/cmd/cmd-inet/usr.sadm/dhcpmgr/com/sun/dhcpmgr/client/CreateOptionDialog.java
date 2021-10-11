/*
 * @(#)CreateOptionDialog.java	1.9	99/08/05 SMI
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

import com.sun.dhcpmgr.server.*;
import com.sun.dhcpmgr.data.*;
import com.sun.dhcpmgr.ui.*;
import com.sun.dhcpmgr.bridge.NotRunningException;


/**
 * Dialog to create/duplicate/edit an option.
 */
public class CreateOptionDialog extends JDialog implements ButtonPanelListener {
    public static final int CREATE = 0;
    public static final int EDIT = 1;
    public static final int DUPLICATE = 2;
    
    private int mode = CREATE;
    private OptionNameField name;
    private JComboBox category;
    private IntegerField code;
    private JComboBox type;
    private JList classList;
    private NoSpaceField clientClass;
    private IntegerField granularity;
    private IntegerField maximum;
    private JCheckBox signalBox;
    private Vector listeners;
    private Option option, originalOption;
    private ButtonPanel buttonPanel;
    private ClassListModel classListModel;
    private JButton add, delete;
    private UpButton moveUp;
    private DownButton moveDown;
    private static final String [] categories = {
	ResourceStrings.getString("option_contexts1"),
	ResourceStrings.getString("option_contexts2"),
	ResourceStrings.getString("option_contexts3")
    };
    private static final String [] types = {
	ResourceStrings.getString("option_types0"),
	ResourceStrings.getString("option_types1"),
	ResourceStrings.getString("option_types2"),
	ResourceStrings.getString("option_types3"),
	ResourceStrings.getString("option_types4")
    };
    
    // Model for the list of vendor classes
    class ClassListModel extends AbstractListModel {
    
	public ClassListModel() {
	    super();
	}
	
	public int getSize() {
	    return option.getVendorCount();
	}
	
	public Object getElementAt(int index) {
	    return option.getVendorAt(index);
	}

	public void addElement(String v) throws ValidationException {
	    option.addVendor(v);
	    fireIntervalAdded(this, option.getVendorCount()-1,
		option.getVendorCount()-1);
	}
	
	public void removeElementAt(int index) {
	    option.removeVendorAt(index);
	    fireIntervalRemoved(this, index, index);
	}
	
	public void moveUp(int index) {
	    String t = (String)option.getVendorAt(index-1);
	    option.setVendorAt(option.getVendorAt(index), index-1);
	    option.setVendorAt(t, index);
	    fireContentsChanged(this, index-1, index);
	}
	
	public void moveDown(int index) {
	    String t = (String)option.getVendorAt(index+1);
	    option.setVendorAt(option.getVendorAt(index), index+1);
	    option.setVendorAt(t, index);
	    fireContentsChanged(this, index, index+1);
	}
	
	public void reset() {
	    fireContentsChanged(this, 0, getSize());
	}
    }	

    public CreateOptionDialog(Frame f, int mode) {
	super(f);
	setLocationRelativeTo(f);

        JPanel classPanel;
    	JLabel maximumLabel, granularityLabel;

	listeners = new Vector();
	
	this.mode = mode;
	switch (mode) {
	case CREATE:
	    setTitle(ResourceStrings.getString("create_option_title"));
	    break;
	case EDIT:
	    setTitle(ResourceStrings.getString("edit_option_title"));
	    break;
	case DUPLICATE:
	    setTitle(ResourceStrings.getString("duplicate_macro_title"));
	    break;
	default:
	    break;
	}
	
	getContentPane().setLayout(new BoxLayout(getContentPane(),
	    BoxLayout.Y_AXIS));
	
	JPanel mainPanel = new JPanel(new BorderLayout());
	mainPanel.setBorder(BorderFactory.createEmptyBorder(10, 10, 10, 10));
	
	JPanel fieldPanel = new JPanel();
	GridBagLayout bag = new GridBagLayout();
	fieldPanel.setLayout(bag);

	// Initialize constraints
	GridBagConstraints c = new GridBagConstraints();
	c.gridx = c.gridy = 0;
	c.gridwidth = c.gridheight = 1;
	c.fill = GridBagConstraints.HORIZONTAL;
	c.insets = new Insets(5, 5, 5, 5);
	c.weightx = c.weighty = 1.0;

	// Label and text field for name
	JLabel l = new JLabel(ResourceStrings.getString("name_label"),
	    SwingConstants.RIGHT);
	bag.setConstraints(l, c);
	fieldPanel.add(l);
	name = new OptionNameField("");
	++c.gridx;
	bag.setConstraints(name, c);
	fieldPanel.add(name);
	
	// Label and combo box for category
	l = new JLabel(ResourceStrings.getString("category_label"),
	    SwingConstants.RIGHT);
	c.gridx = 0;
	++c.gridy;
	bag.setConstraints(l, c);
	fieldPanel.add(l);
	category = new JComboBox(categories);
	category.setEditable(false);
	++c.gridx;
	bag.setConstraints(category, c);
	fieldPanel.add(category);
	
	// Label and text field for code
	l = new JLabel(ResourceStrings.getString("option_code_label"),
	    SwingConstants.RIGHT);
	c.gridx = 0;
	++c.gridy;
	bag.setConstraints(l, c);
	fieldPanel.add(l);
	code = new IntegerField();
	++c.gridx;
	bag.setConstraints(code, c);
	fieldPanel.add(code);
	
	// Label and combo box for data type
	l = new JLabel(ResourceStrings.getString("data_type_label"),
	    SwingConstants.RIGHT);
	c.gridx = 0;
	++c.gridy;
	bag.setConstraints(l, c);
	fieldPanel.add(l);
	type = new JComboBox(types);
	type.setEditable(false);
	++c.gridx;
	bag.setConstraints(type, c);
	fieldPanel.add(type);
	
	// Label and text field for granularity
	granularityLabel = new JLabel(
	    ResourceStrings.getString("granularity_label"),
	    SwingConstants.RIGHT);
	c.gridx = 0;
	++c.gridy;
	bag.setConstraints(granularityLabel, c);
	fieldPanel.add(granularityLabel);
	granularity = new IntegerField(5);
	++c.gridx;
	bag.setConstraints(granularity, c);
	fieldPanel.add(granularity);
	
	// Label and text field for maximum
	maximumLabel = new JLabel(ResourceStrings.getString("maximum_label"),
	    SwingConstants.RIGHT);
	c.gridx = 0;
	++c.gridy;
	bag.setConstraints(maximumLabel, c);
	fieldPanel.add(maximumLabel);
	maximum = new IntegerField(5);
	++c.gridx;
	bag.setConstraints(maximum, c);
	fieldPanel.add(maximum);
	
	mainPanel.add(fieldPanel, BorderLayout.WEST);
	
	// Editing controls for client classes
	bag = new GridBagLayout();
	classPanel = new JPanel(bag);
	Border tb = BorderFactory.createTitledBorder(
	    BorderFactory.createLineBorder(Color.black),
	    ResourceStrings.getString("client_classes_label"));
	classPanel.setBorder(BorderFactory.createCompoundBorder(tb,
	    BorderFactory.createEmptyBorder(5, 5, 5, 5)));
	
	c = new GridBagConstraints();
	c.gridx = c.gridy = 0;
	c.weightx = c.weighty = 1.0;
	c.gridheight = 1;
	c.gridwidth = 1;
	
	// Field to type in new classes
	clientClass = new NoSpaceField("", 20);
	c.fill = GridBagConstraints.HORIZONTAL;
	bag.setConstraints(clientClass, c);
	classPanel.add(clientClass);

	// Button for Add operation
	add = new JButton(ResourceStrings.getString("add"));
	c.fill = GridBagConstraints.NONE;
	++c.gridx;
	c.weightx = 0.5;
	bag.setConstraints(add, c);
	classPanel.add(add);
	
	// List for classes
	classListModel = new ClassListModel();
	classList = new JList(classListModel);

	// Make sure it's approximately wide enough for our purposes, 20 chars
	classList.setPrototypeCellValue("abcdefghijklmnopqrst");
	classList.setSelectionMode(
	    ListSelectionModel.MULTIPLE_INTERVAL_SELECTION);
	JScrollPane scrollPane = new JScrollPane(classList);
	c.fill = GridBagConstraints.BOTH;
	c.gridx = 0;
	++c.gridy;
	c.weightx = 1.0;
	bag.setConstraints(scrollPane, c);
	classPanel.add(scrollPane);
	
	// Buttons to manipulate the list contents
	JPanel editButtonPanel = new JPanel(new VerticalButtonLayout());
	moveUp = new UpButton();
	editButtonPanel.add(moveUp);
	moveDown = new DownButton();
	editButtonPanel.add(moveDown);
	delete = new JButton(ResourceStrings.getString("delete"));
	editButtonPanel.add(delete);
	++c.gridx;
	c.weightx = 0.5;
	bag.setConstraints(editButtonPanel, c);
	classPanel.add(editButtonPanel);
	
	/*
	 * Disable all buttons to start; selection changes will adjust button
	 * state as necessary
	 */
	add.setEnabled(false);
	delete.setEnabled(false);
	moveUp.setEnabled(false);
	moveDown.setEnabled(false);
	
	// Create listener for button presses, take action as needed
	ActionListener al = new ActionListener() {
	    public void actionPerformed(ActionEvent e) {
		if (e.getSource() == add || e.getSource() == clientClass) {
		    try {
		        classListModel.addElement(clientClass.getText());
		    } catch (ValidationException ex) {
		        // Something wrong with class name
			MessageFormat form = new MessageFormat(
			    ResourceStrings.getString("invalid_client_class"));
			Object [] args = new Object[] { clientClass.getText() };
			JOptionPane.showMessageDialog(CreateOptionDialog.this,
			    form.format(args),
			    ResourceStrings.getString("input_error"),
			    JOptionPane.ERROR_MESSAGE);
			return;
		    }
		} else if (e.getSource() == delete) {
		    int [] indices = classList.getSelectedIndices();
		    if (indices.length > 1) {
			/*
			 * Need to sort them so that the delete's don't
			 * interfere with each other
			 */
			for (int i = 0; i < indices.length; ++i) {
			    for (int j = i; j < indices.length; ++j) {
				if (indices[i] > indices[j]) {
				    int k = indices[i];
				    indices[i] = indices[j];
				    indices[j] = k;
				}
			    }
			}
		    }
		    // Now delete from high index to low
		    for (int i = indices.length - 1; i >= 0; --i) {
			classListModel.removeElementAt(indices[i]);
		    }
		    if (indices.length > 1) {
			// Clear selection if multiple deleted
			classList.clearSelection();
			/*
			 * XXX We don't get a selection event for some reason,
			 * make it work for now
			 */
			delete.setEnabled(false);
		    } else {
			// Make sure to select something in the list
			if (classListModel.getSize() == 0) {
			    // List is empty, so disable delete
			    delete.setEnabled(false);
			} else if (indices[0] >= classListModel.getSize()) {
			    // Select last one if we're off the end
			    classList.setSelectedIndex(
				classListModel.getSize()-1);
			} else {
			    // Select next one in list
			    classList.setSelectedIndex(indices[0]);
			}
		    }
		} else if (e.getSource() == moveUp) {
		    int i = classList.getSelectedIndex();
		    classListModel.moveUp(i);
		    // Keep item selected so repeated moveUp's affect same item
		    classList.setSelectedIndex(i-1);
		} else if (e.getSource() == moveDown) {
		    int i = classList.getSelectedIndex();
		    classListModel.moveDown(i);
		    // Keep item selected so repeated moveDowns affect same item
		    classList.setSelectedIndex(i+1);
		}
	    }
	};
	clientClass.addActionListener(al);
	add.addActionListener(al);
	delete.addActionListener(al);
	moveUp.addActionListener(al);
	moveDown.addActionListener(al);
	
	// Put a selection listener on the list to enable buttons appropriately
	classList.addListSelectionListener(new ListSelectionListener() {
	    public void valueChanged(ListSelectionEvent e) {
		int [] indices = classList.getSelectedIndices();
		switch (indices.length) {
		case 0:
		    // Nothing selected; disable them all
		    delete.setEnabled(false);
		    moveUp.setEnabled(false);
		    moveDown.setEnabled(false);
		    break;
		case 1:
		    delete.setEnabled(true);
		    // Can't move first one up
		    moveUp.setEnabled(indices[0] != 0);
		    // Can't move last one down
		    if (indices[0] == (classListModel.getSize() - 1)) {
			moveDown.setEnabled(false);
		    } else {
			moveDown.setEnabled(true);
		    }
		    break;
		default:
		    // More than one; only delete is allowed
		    delete.setEnabled(true);
		    moveUp.setEnabled(false);
		    moveDown.setEnabled(false);
		}
	    }
	});
	// Enable Add when class is not empty.
	clientClass.getDocument().addDocumentListener(new DocumentListener() {
	    public void insertUpdate(DocumentEvent e) {
		add.setEnabled(clientClass.getText().length() != 0);
	    }
	    public void changedUpdate(DocumentEvent e) {
		insertUpdate(e);
	    }
	    public void removeUpdate(DocumentEvent e) {
		insertUpdate(e);
	    }
	});

	mainPanel.add(classPanel, BorderLayout.CENTER);
	
	signalBox = new JCheckBox(ResourceStrings.getString("signal_server"),
	    true);
	signalBox.setHorizontalAlignment(SwingConstants.CENTER);
	JPanel signalPanel = new JPanel();
	signalPanel.add(signalBox);
	mainPanel.add(signalPanel, BorderLayout.SOUTH);
	
	getContentPane().add(mainPanel);
	getContentPane().add(new JSeparator());
	
	buttonPanel = new ButtonPanel(true);
	buttonPanel.addButtonPanelListener(this);
	getContentPane().add(buttonPanel);
	
	setOption(new Option());
	
	if (mode == EDIT) {
	    buttonPanel.setOkEnabled(true);
	}

	// Enable OK when there is data in the name field
	name.getDocument().addDocumentListener(new DocumentListener() {
	    public void insertUpdate(DocumentEvent e) {
		buttonPanel.setOkEnabled(e.getDocument().getLength() != 0);
	    }
	    public void changedUpdate(DocumentEvent e) {
		insertUpdate(e);
	    }
	    public void removeUpdate(DocumentEvent e) {
		insertUpdate(e);
	    }
	});
	
	// If category != VENDOR you can't mess with the client class data
	category.addItemListener(new ItemListener() {
	    public void itemStateChanged(ItemEvent e) {
		boolean isVendor =
		    (category.getSelectedIndex() == (Option.VENDOR-1));
		if (!isVendor) {
		    option.clearVendors();
		    clientClass.setText("");
		}
		clientClass.setEnabled(isVendor);
		classList.setEnabled(isVendor);		
	    }
	});
	
	// Update state of granularity & maximum depending on data type selected
	type.addItemListener(new ItemListener() {
	    public void itemStateChanged(ItemEvent e) {
		switch (type.getSelectedIndex()) {
		case Option.ASCII:
		case Option.OCTET:
		    granularity.setText("1");
		    granularity.setEditable(false);
		    maximum.setEditable(true);
		    break;
		case Option.BOOLEAN:
		    granularity.setText("0");
		    granularity.setEditable(false);
		    maximum.setText("0");
		    maximum.setEditable(false);
		    break;
		case Option.NUMBER:
		case Option.IP:
		    granularity.setEditable(true);
		    maximum.setEditable(true);
		    break;
		}
	    }
	});
    }
    
    public void setOption(Option o) {
	originalOption = o; // Keep a copy so reset will work
	option = (Option)o.clone();
	resetValues();
    }
    
    private void resetValues() {
	if (mode == DUPLICATE) {
	    name.setText("");
	} else {
	    name.setText(option.getKey());
	}
	category.setSelectedIndex(option.getContext()-1);
	code.setValue(option.getCode());
	type.setSelectedIndex(option.getType());
	granularity.setValue(option.getGranularity());
	maximum.setValue(option.getMaximum());
	classListModel.reset();
	signalBox.setSelected(true);
    }
    
    public void buttonPressed(int buttonId) {
	switch (buttonId) {
	case OK:
	    try {
		option.setKey(name.getText());
		option.setContext((byte)(category.getSelectedIndex()+1));
		option.setCode((short)code.getValue());
		option.setType((byte)type.getSelectedIndex());
		option.setGranularity(granularity.getValue());
		option.setMaximum(maximum.getValue());
		if (option.getContext() == Option.VENDOR &&
			option.getVendorCount() == 0) {
		    JOptionPane.showMessageDialog(this,
		    	ResourceStrings.getString("empty_vendor_error"),
		    	ResourceStrings.getString("server_error_title"),
		    	JOptionPane.ERROR_MESSAGE);
		    return;
		}
		DhcptabMgr server = DataManager.get().getDhcptabMgr();
		if ((mode == CREATE) || (mode == DUPLICATE)) {
		    server.createRecord(option, signalBox.isSelected());
		} else if (mode == EDIT) {
		    server.modifyRecord(originalOption, option,
			signalBox.isSelected());
		}
		fireActionPerformed();
		setVisible(false);
		dispose();
	    } catch (NotRunningException e) {
		// Server not running, put up a warning
		JOptionPane.showMessageDialog(this,
		    ResourceStrings.getString("server_not_running"),
		    ResourceStrings.getString("warning"),
		    JOptionPane.WARNING_MESSAGE);
	        fireActionPerformed();
		setVisible(false);
		dispose();
    	    } catch (Exception e) {
		MessageFormat form = null;
		Object [] args = new Object[2];
		switch (mode) {
		case CREATE:
		case DUPLICATE:
		    form = new MessageFormat(
			ResourceStrings.getString("create_option_error"));
		    args[0] = option.getKey();
		    break;
		case EDIT:
		    form = new MessageFormat(
			ResourceStrings.getString("edit_option_error"));
		    args[0] = originalOption.getKey();
		    break;
		}
		args[1] = e.getMessage();
		JOptionPane.showMessageDialog(this, form.format(args),
		    ResourceStrings.getString("server_error_title"),
		    JOptionPane.ERROR_MESSAGE);
	    }
	    break;
	case CANCEL:
	    setVisible(false);
	    dispose();
	    break;
	case HELP:
	    String helpTag = null;
	    switch (mode) {
	    case CREATE:
		helpTag = "create_option";
		break;
	    case DUPLICATE:
		helpTag = "duplicate_option";
		break;
	    case EDIT:
		helpTag = "modify_option";
		break;
	    }
	    DhcpmgrApplet.showHelp(helpTag);
	    break;
	case RESET:
	    setOption(originalOption);
	    break;
	}
    }
    
    public void addActionListener(ActionListener l) {
	listeners.addElement(l);
    }
    
    public void removeActionListener(ActionListener l) {
	listeners.removeElement(l);
    }
    
    protected void fireActionPerformed() {
	String command = null;
	switch (mode) {
	case CREATE:
	    command = DialogActions.CREATE;
	case DUPLICATE:
	    command = DialogActions.DUPLICATE;
	    break;
	case EDIT:
	    command = DialogActions.EDIT;
	    break;
	}
	ActionEvent e = new ActionEvent(this, ActionEvent.ACTION_PERFORMED,
	    command);
	Enumeration en = listeners.elements();
	while (en.hasMoreElements()) {
	    ActionListener l = (ActionListener)en.nextElement();
	    l.actionPerformed(e);
	}
    }
}
