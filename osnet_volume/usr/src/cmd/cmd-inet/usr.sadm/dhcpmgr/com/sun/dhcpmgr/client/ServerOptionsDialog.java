/*
 * @(#)ServerOptionsDialog.java	1.6	99/10/12 SMI
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
import java.text.*;
import java.util.*;

import com.sun.dhcpmgr.server.*;
import com.sun.dhcpmgr.data.*;
import com.sun.dhcpmgr.ui.*;

/**
 * Dialog to edit the options for the server as stored in the startup script.
 */
public class ServerOptionsDialog extends DhcpmgrDialog {
    private static int DEFAULT_RESCAN_INTERVAL = 60;
    private DhcpdOptions options, originalOptions;
    private JCheckBox verboseLogging, detectDuplicates, restartServer,
        logTransactions, reloadEnabled;
    private IntegerField relayHops, reloadInterval, cacheTime;
    private JRadioButton noBootp, autoBootp, manualBootp;
    private IPAddressList serverList;
    private JComboBox logFacility;
    private JTable monitoredTable, ignoredTable;
    private LeftButton leftButton;
    private RightButton rightButton;
    
    /*
     * Model for the tables which are used to edit the lists of
     * interfaces which are monitored and ignored.
     */
    class InterfaceTableModel extends AbstractTableModel {
	private Vector interfaces;
	
	public InterfaceTableModel() {
	    interfaces = new Vector();
	}
	
	// Initialize the list of interfaces
	public void setInterfaceList(IPInterface [] ifs) {
	    interfaces.removeAllElements();
	    if (ifs != null) {
		for (int i = 0; i < ifs.length; ++i) {
		    interfaces.addElement(ifs[i]);
		}
	    }
	    fireTableDataChanged();
	}
	
	// Retrieve the interfacess as a comma-separated list
	public String getInterfaceList() {
	    StringBuffer b = new StringBuffer();
	    Enumeration e = interfaces.elements();
	    while (e.hasMoreElements()) {
		if (b.length() != 0) {
		    b.append(',');
		}
		IPInterface ipif = (IPInterface)e.nextElement();
		b.append(ipif.getName());
	    }
	    return b.toString();
	}
	
	// Retrieve interface object for named interface
	public IPInterface getInterface(String name) {
	    Enumeration e = interfaces.elements();
	    while (e.hasMoreElements()) {
		IPInterface ipif = (IPInterface)e.nextElement();
		if (name.equals(ipif.getName())) {
		    return ipif;
		}
	    }
	    return null;
	}
	
	// Retrieve the interface object at a particular row in the table
	public IPInterface getInterfaceAt(int row) {
	    return (IPInterface)interfaces.elementAt(row);
	}
	
	// Add an interface to the table
	public void addInterface(IPInterface ipif) {
	    interfaces.addElement(ipif);
	    fireTableDataChanged();
	}
	
	// Delete an interface from the table
	public void deleteInterface(IPInterface ipif) {
	    interfaces.removeElement(ipif);
	    fireTableDataChanged();
	}
	
	// Return number of rows
	public int getRowCount() {
	    return interfaces.size();
	}
	
	// Always two columns: interface name and network
	public int getColumnCount() {
	    return 2;
	}
	
	// Return cell value at a particular coordinate
	public Object getValueAt(int row, int column) {
	    IPInterface ipif = (IPInterface)interfaces.elementAt(row);
	    if (column == 0) {
		return ipif.getName();
	    } else {
		return ipif.getNetwork().toString();
	    }
	}
	
	// All data is strings from the display's point of view
	public Class getColumnClass(int column) {
	    return String.class;
	}
	
	// Get headings for each column
	public String getColumnName(int column) {
	    if (column == 0) {
		return ResourceStrings.getString("service_options_interface");
	    } else {
		return ResourceStrings.getString("service_options_network");
	    }
	}
    }
    
    public ServerOptionsDialog(Frame f, DhcpdOptions opts) {
	super(f, true); // We want a reset button
	setOptions(opts);
	resetValues();
    }

    /**
     * Provide a title to be displayed for the dialog
     */
    public String getTitle() {
	return ResourceStrings.getString("service_options_title");
    }

    /**
     * Construct and return the main display for this dialog.
     */
    protected JPanel getMainPanel() {
	
	JPanel mainPanel = new JPanel(new BorderLayout());

	/*
	 * Start with a tabbed view; the top tab is the options for the
	 * daemon, the lower tab is the interfaces to be monitored.
	 */
	JTabbedPane tabbedPane = new JTabbedPane();
	JPanel optionsPanel = new JPanel();
	optionsPanel.setBorder(BorderFactory.createEmptyBorder(10, 10, 10, 10));

	// Use a gridbag with equal weights all around, cells anchored to west
	GridBagLayout bag = new GridBagLayout();
	GridBagConstraints con = new GridBagConstraints();
	con.gridx = con.gridy = 0;
	con.weightx = con.weighty = 1.0;
	con.anchor = GridBagConstraints.WEST;
	con.insets = new Insets(2, 2, 2, 2);
	
	optionsPanel.setLayout(bag);
	
	// Add control for number of hops allowed
	Box box = Box.createHorizontalBox();
	box.add(new JLabel(
	    ResourceStrings.getString("service_options_hops")));
	box.add(Box.createHorizontalStrut(5));
	relayHops = new IntegerField();
	box.add(relayHops);
	bag.setConstraints(box, con);
	optionsPanel.add(box);

	// Add control for verbose logging
	verboseLogging = new JCheckBox(
	    ResourceStrings.getString("service_options_verbose"), false);
	++con.gridy;
	bag.setConstraints(verboseLogging, con);
	optionsPanel.add(verboseLogging);

	// Add control for transaction logging on/off and facility to use
	box = Box.createHorizontalBox();
	logTransactions = new JCheckBox(
	ResourceStrings.getString("service_options_log_transactions"),
	false);
	logTransactions.setAlignmentY((float)0.5);
	box.add(logTransactions);
	box.add(Box.createHorizontalStrut(5));
	logFacility = new JComboBox(DhcpdOptions.getLoggingFacilities());
	logFacility.setAlignmentY((float)0.5);
	box.add(logFacility);
	++con.gridy;
	bag.setConstraints(box, con);
	optionsPanel.add(box);
	// Enable logging facility choices only when logging is turned on
	logTransactions.addActionListener(new ActionListener() {
	    public void actionPerformed(ActionEvent e) {
	    	logFacility.setEnabled(logTransactions.isSelected());
	    }
	});

	/*
	 * The main tab has two different displays depending on whether it's
	 * a relay or a full-fledged server.
	 */
	if (!DhcpmgrApplet.modeIsRelay) {
	    // Add control for duplicate detection using ICMP
	    detectDuplicates = new JCheckBox(
		ResourceStrings.getString("service_options_detect_duplicates"),
		true);
	    ++con.gridy;
	    bag.setConstraints(detectDuplicates, con);
	    optionsPanel.add(detectDuplicates);

	    // Add control for automatic reload of dhcptab and period
	    box = Box.createHorizontalBox();
	    reloadEnabled = new JCheckBox(
		ResourceStrings.getString("service_options_reload_dhcptab"));
	    reloadEnabled.setAlignmentY((float)0.5);
	    box.add(reloadEnabled);
	    box.add(Box.createHorizontalStrut(5));
	    reloadInterval = new IntegerField();
	    reloadInterval.setAlignmentY((float)0.5);
	    box.add(reloadInterval);
	    box.add(Box.createHorizontalStrut(5));
	    JLabel label = new JLabel(
		ResourceStrings.getString("service_options_reload_minutes"));
	    label.setAlignmentY((float)0.5);
	    box.add(label);
	    ++con.gridy;
	    bag.setConstraints(box, con);
	    optionsPanel.add(box);

	    // Add control for length of time to cache offers
	    box = Box.createHorizontalBox();
	    box.add(new JLabel(
		ResourceStrings.getString("service_options_cache")));
	    box.add(Box.createHorizontalStrut(5));
	    cacheTime = new IntegerField();
	    box.add(cacheTime);
	    box.add(Box.createHorizontalStrut(5));
	    box.add(new JLabel(
		ResourceStrings.getString("service_options_seconds")));
	    ++con.gridy;
	    bag.setConstraints(box, con);
	    optionsPanel.add(box);

	    // Add choices for BOOTP compatibility behavior: none, auto, manual
	    JPanel panel = new JPanel();
	    panel.setLayout(new GridLayout(3, 1));
	    Border b = BorderFactory.createTitledBorder(
	        BorderFactory.createLineBorder(Color.black),
		ResourceStrings.getString("service_options_bootp_compat"));
	    panel.setBorder(BorderFactory.createCompoundBorder(b,
	        BorderFactory.createEmptyBorder(5, 5, 5, 5)));
	    
	    ButtonGroup bootpCompat = new ButtonGroup();
	    
	    noBootp = new JRadioButton(
		ResourceStrings.getString("service_options_bootp_none"), true);
	    bootpCompat.add(noBootp);
	    panel.add(noBootp);
    
	    autoBootp = new JRadioButton(
		ResourceStrings.getString("service_options_bootp_auto"), false);
	    bootpCompat.add(autoBootp);
	    panel.add(autoBootp);
    
	    manualBootp = new JRadioButton(
		ResourceStrings.getString("service_options_bootp_manual"),
		false);
	    bootpCompat.add(manualBootp);
	    panel.add(manualBootp);
	    
	    ++con.gridy;
	    con.fill = GridBagConstraints.HORIZONTAL;
	    bag.setConstraints(panel, con);
	    optionsPanel.add(panel);

	    // Enable reload interval only when reload option is checked
	    reloadEnabled.addActionListener(new ActionListener() {
	        public void actionPerformed(ActionEvent e) {
	    	    reloadInterval.setEnabled(reloadEnabled.isSelected());
	        }
	    });
	} else {
	    /*
	     * In relay mode the only other thing we can control is list of
	     * servers which we forward requests to.
	     */
	    serverList = new IPAddressList();
	    Border tb = BorderFactory.createTitledBorder(
		BorderFactory.createLineBorder(Color.black),
		ResourceStrings.getString("dhcp_servers"));
	    serverList.setBorder(BorderFactory.createCompoundBorder(tb,
		BorderFactory.createEmptyBorder(5, 5, 5, 5)));
	    ++con.gridy;
	    bag.setConstraints(serverList, con);
	    optionsPanel.add(serverList);
	}
	
	tabbedPane.addTab(ResourceStrings.getString("service_options_options"),
	    optionsPanel);		
	
	// Panel for interfaces
	monitoredTable = new JTable(new InterfaceTableModel());
	ignoredTable = new JTable(new InterfaceTableModel());
	monitoredTable.setSelectionMode(
	    ListSelectionModel.MULTIPLE_INTERVAL_SELECTION);
	ignoredTable.setSelectionMode(
	    ListSelectionModel.MULTIPLE_INTERVAL_SELECTION);
	
	Box interfaceBox = Box.createHorizontalBox();
	
	// The list of interfaces we're monitoring goes on the left
	JPanel panel = new JPanel(new BorderLayout(5, 5));
	panel.add(new JLabel(
	    ResourceStrings.getString("service_options_monitored")),
	    BorderLayout.NORTH);
	JScrollPane scrollPane = new JScrollPane(monitoredTable);
	Dimension d = monitoredTable.getPreferredScrollableViewportSize();
	d.height = 100;
	d.width = 210;
	monitoredTable.setPreferredScrollableViewportSize(d);
	panel.add(scrollPane, BorderLayout.CENTER);
	interfaceBox.add(panel);
	interfaceBox.add(Box.createHorizontalStrut(10));
	
	// The buttons to move items between the lists go in the middle
	panel = new JPanel(new VerticalButtonLayout());
	leftButton = new LeftButton();
	rightButton = new RightButton();
	rightButton.setEnabled(false);
	leftButton.setEnabled(false);
	panel.add(rightButton);
	panel.add(leftButton);
	interfaceBox.add(panel);
	interfaceBox.add(Box.createHorizontalStrut(10));
	
	// The list of interfaces to ignore is on the right
	panel = new JPanel(new BorderLayout(5, 5));
	panel.add(new JLabel(
	    ResourceStrings.getString("service_options_ignored")),
	    BorderLayout.NORTH);
	scrollPane = new JScrollPane(ignoredTable);
	d = ignoredTable.getPreferredScrollableViewportSize();
	d.height = 100;
	d.width = 210;
	ignoredTable.setPreferredScrollableViewportSize(d);
	panel.add(scrollPane, BorderLayout.CENTER);
	interfaceBox.add(panel);

	// Now create the tab for the interface manipulation
	panel = new JPanel(new BorderLayout());
	panel.setBorder(BorderFactory.createEmptyBorder(10, 10, 10, 10));
	panel.add(interfaceBox, BorderLayout.CENTER);
	tabbedPane.addTab(
	    ResourceStrings.getString("service_options_interfaces"), panel);

	JPanel borderPanel = new JPanel(new BorderLayout());
	borderPanel.setBorder(BorderFactory.createEmptyBorder(5, 5, 5, 5));
	borderPanel.add(tabbedPane, BorderLayout.CENTER);
	
	mainPanel.add(borderPanel, BorderLayout.NORTH);
	
	/*
	 * Allow them to specify server should be restarted when these changes
	 * are applied
	 */
	restartServer = new JCheckBox(
	    ResourceStrings.getString("service_options_restart"));
	panel = new JPanel(new FlowLayout(FlowLayout.LEFT));
	panel.add(restartServer);
	mainPanel.add(panel, BorderLayout.CENTER);
	
	buttonPanel.setOkEnabled(true);
	
	// Handle enable and disable of buttons based on selection state
	monitoredTable.getSelectionModel().addListSelectionListener(
		new ListSelectionListener() {
	    public void valueChanged(ListSelectionEvent e) {
		if (monitoredTable.getSelectedRowCount() != 0) {
		    rightButton.setEnabled(true);
		    ignoredTable.getSelectionModel().clearSelection();
		} else {
		    rightButton.setEnabled(false);
		}
	    }
	});
	
	ignoredTable.getSelectionModel().addListSelectionListener(
		new ListSelectionListener() {
	    public void valueChanged(ListSelectionEvent e) {
		if (ignoredTable.getSelectedRowCount() != 0) {
		    leftButton.setEnabled(true);
		    monitoredTable.getSelectionModel().clearSelection();
		} else {
		    leftButton.setEnabled(false);
		}
	    }
	});
	
	// Handle button presses
	rightButton.addActionListener(new ActionListener() {
	    public void actionPerformed(ActionEvent e) {
		int [] rows = monitoredTable.getSelectedRows();
		if (rows == null) {
		    return;
		}
		InterfaceTableModel monitoredModel =
		    (InterfaceTableModel)monitoredTable.getModel();
		InterfaceTableModel ignoredModel =
		    (InterfaceTableModel)ignoredTable.getModel();
		/*
		 * Now do the adds, then the removes; otherwise the row numbers
		 * we just got might be wrong
		 */
		Vector removals = new Vector();
		for (int i = 0; i < rows.length; ++i) {
		    IPInterface ipif = monitoredModel.getInterfaceAt(rows[i]);
		    ignoredModel.addInterface(ipif);
		    removals.addElement(ipif);
		}
		Enumeration en = removals.elements();
		while (en.hasMoreElements()) {
		    monitoredModel.deleteInterface(
			(IPInterface)en.nextElement());
		}
		/*
		 * Clear the selection; this prevents exceptions from selection
		 * pointing at rows that are gone
		 */
		monitoredTable.getSelectionModel().clearSelection();
	    }
	});
	
	leftButton.addActionListener(new ActionListener() {
	    public void actionPerformed(ActionEvent e) {
		int [] rows = ignoredTable.getSelectedRows();
		if (rows == null) {
		    return;
		}
		InterfaceTableModel monitoredModel =
		    (InterfaceTableModel)monitoredTable.getModel();
		InterfaceTableModel ignoredModel =
		    (InterfaceTableModel)ignoredTable.getModel();
		/*
		 * Now do the adds, then the removes; otherwise the row numbers
		 * we just got might be wrong
		 */
		Vector removals = new Vector();
		for (int i = 0; i < rows.length; ++i) {
		    IPInterface ipif = ignoredModel.getInterfaceAt(rows[i]);
		    monitoredModel.addInterface(ipif);
		    removals.addElement(ipif);
		}
		Enumeration en = removals.elements();
		while (en.hasMoreElements()) {
		    ignoredModel.deleteInterface((IPInterface)en.nextElement());
		}
		/*
		 * Clear the selection; this prevents exceptions from selection
		 * pointing at rows that are gone
		 */
		ignoredTable.getSelectionModel().clearSelection();
	    }
	});


	return mainPanel;
    }
    
    // Save a copy of the option settings so reset can work
    private void setOptions(DhcpdOptions o) {
	originalOptions = (DhcpdOptions)o.clone();
	options = o;
    }
    
    // Reset all controls to initial values
    private void resetValues() {
	// Main tab parameters; first verbose logging
	verboseLogging.setSelected(options.isVerbose());
	// Relay hops
	if (options.isRelayHops()) {
	    relayHops.setValue(options.getRelayHops());
	} else {
	    relayHops.setValue(DhcpdOptions.DEFAULT_HOPS);
	}
	// Set logging controls
	logTransactions.setSelected(options.isLogging());
	logFacility.setEnabled(options.isLogging());
	if (options.isLogging()) {
	    logFacility.setSelectedItem(options.getLogging());
	} else {
	    logFacility.setSelectedIndex(0);
	}

	if (!DhcpmgrApplet.modeIsRelay) {
	    // Set bootp compat. controls
	    noBootp.setSelected(!options.isBootpCompatible());
	    if (options.isBootpCompatible()) {
		autoBootp.setSelected(options.isBootpAutomatic());
		manualBootp.setSelected(!options.isBootpAutomatic());
	    }
	    detectDuplicates.setSelected(options.isICMPVerify());
	    reloadEnabled.setSelected(options.isRescan());
	    reloadInterval.setEnabled(options.isRescan());

	    // Set rescan interval to default if it's not specified
	    if (options.isRescan()) {
		reloadInterval.setValue(options.getRescan());
	    } else {
		reloadInterval.setValue(DEFAULT_RESCAN_INTERVAL);
	    }
	    if (options.isOfferTtl()) {
		cacheTime.setValue(options.getOfferTtl());
	    } else {
		cacheTime.setValue(DhcpdOptions.DEFAULT_OFFER_TTL);
	    }
	} else {
	    // In relay case only the server list is available
	    serverList.setAddressList(options.getRelay());
	}
	
	// Interfaces tab
	try {
	    IPInterface [] interfaces =
		DataManager.get().getDhcpServiceMgr().getInterfaces();
	    InterfaceTableModel monitoredModel =
		(InterfaceTableModel)monitoredTable.getModel();
	    InterfaceTableModel ignoredModel =
		(InterfaceTableModel)ignoredTable.getModel();
	    if (options.isInterfaces()) {
		ignoredModel.setInterfaceList(interfaces);
		monitoredModel.setInterfaceList(null);
		StringTokenizer st =
		    new StringTokenizer(options.getInterfaces(), ",");
		while (st.hasMoreTokens()) {
		    IPInterface ipif =
			ignoredModel.getInterface(st.nextToken());
		    if (ipif != null) {
			monitoredModel.addInterface(ipif);
			ignoredModel.deleteInterface(ipif);
		    }
		}
	    } else {
		monitoredModel.setInterfaceList(interfaces);
		ignoredModel.setInterfaceList(null); 
	    }
	} catch (Throwable e) {
	    e.printStackTrace();
	}

	// Default to restarting server
	restartServer.setSelected(true);
    }
    
    /**
     * User pressed OK, do what we think is necessary
     */
    protected void doOk() {
	try {
	    options.setVerbose(verboseLogging.isSelected());
	    if (relayHops.getValue() != DhcpdOptions.DEFAULT_HOPS) {
	        options.setRelayHops(true, new Integer(relayHops.getValue()));
	    } else {
	        options.setRelayHops(false, null);
	    }
	    options.setLogging(logTransactions.isSelected(),
		(Integer)logFacility.getSelectedItem());

	    if (!DhcpmgrApplet.modeIsRelay) {
	        options.setBootpCompatible(!noBootp.isSelected(),
		    autoBootp.isSelected());
	        options.setICMPVerify(detectDuplicates.isSelected());
	        if (reloadEnabled.isSelected() &&
			reloadInterval.getValue() != 0) {
		    options.setRescan(true,
		        new Integer(reloadInterval.getValue()));
	        } else {
		    options.setRescan(false, null);
	        }
	        if (cacheTime.getValue() != DhcpdOptions.DEFAULT_OFFER_TTL) {
		    options.setOfferTtl(true,
		        new Integer(cacheTime.getValue()));
	        } else {
		    options.setOfferTtl(false, null);
	        }
	    } else {
	        options.setRelay(true, serverList.getAddressListString());
	    }
	    if (monitoredTable.getRowCount() == 0) {
	        // XXX Need to disable OK when this is the case
	        return;
	    }
	    if (ignoredTable.getRowCount() != 0) {
	        /*
	         * If nothing is ignored then let server default to all
	         * interfaces
	         */
	        options.setInterfaces(true, ((InterfaceTableModel)
		    monitoredTable.getModel()).getInterfaceList());
	    } else {
	        options.setInterfaces(false, null);
	    }
	    DataManager.get().getDhcpServiceMgr().writeDefaults(options);
	    if (restartServer.isSelected()) {
	        DataManager.get().getDhcpServiceMgr().shutdown();
	        // Wait 5 secs for server to try to shutdown
	        Thread.sleep(5000);
	        DataManager.get().getDhcpServiceMgr().startup();
	    }
	    fireActionPerformed();
	    setVisible(false);
	    dispose();
        } catch (Exception e) {
	    MessageFormat form = null;
	    Object [] args = new Object[2];
	    form = new MessageFormat(
	        ResourceStrings.getString("service_options_error"));
	    args[0] = e.getMessage();
	    JOptionPane.showMessageDialog(this, form.format(args),
	        ResourceStrings.getString("server_error_title"),
	        JOptionPane.ERROR_MESSAGE);
        }
    }

    /**
     * Return help system lookup key
     */
    protected String getHelpKey() {
	if (DhcpmgrApplet.modeIsRelay) {
	    return "modify_relay";
	} else {
	    return "modify_server";
	}
    }

    /**
     * User pressed reset; go back to starting value
     */
    protected void doReset() {
	setOptions(originalOptions);
	resetValues();
    }
    
    /**
     * Notify our invoker that we're done
     */
    protected void fireActionPerformed() {
	fireActionPerformed(this, DialogActions.OK);
    }
}
