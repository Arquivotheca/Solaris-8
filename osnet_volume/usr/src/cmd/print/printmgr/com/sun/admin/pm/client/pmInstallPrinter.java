/*
 *
 * ident	"@(#)pmInstallPrinter.java	1.8	99/09/24 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmInstallPrinter.java
 * Install and Modify Printer implementation
 */

package com.sun.admin.pm.client;

import java.awt.*;
import java.awt.event.*;
import java.util.Vector;
import javax.swing.JPanel;
import javax.swing.*;
import javax.swing.JTable;
import javax.swing.ListSelectionModel;
import javax.swing.event.ListSelectionListener;
import javax.swing.event.ListSelectionEvent;
import javax.swing.table.AbstractTableModel;

import com.sun.admin.pm.server.*;


/*
 * Window for Edit -> Install Printer
 */

public class pmInstallPrinter extends pmInstallScreen {

	int action;
	pmTop myTop;
	pmFrame frame;
	JScrollPane scrollPane;
	String printer = null;
	String server = null;
	String description = null;
	String port = null;
	String ptype = null;
	String fcontents = null;
	String faultn = null;
	String destination = null;
	String protocol = null;
	String useraccesslist[] = null;
	String oldptype = null;
	String olddevice = null;
	Printer workingPrinter;

	JList accessList;
	JScrollPane ascrollPane;
	accessListModel accessModel;

	String helpTag = null;

	String cmdLog = null;
	String errorLog = null;
	String warnLog = null;

	String actionName = null;

	// where to place initial focus 
   	Component defaultComponent = null;
 
    public pmInstallPrinter(pmTop myTop, int action) {
        boolean failed = false;
        
	this.myTop = myTop;
	this.action = action;
	workingPrinter = new Printer(myTop.ns);

	switch (action) {

	case Constants.ADDLOCAL:

	    Debug.message("CLNT:pmInstPr: ADD LOCAL");
	    frame = new pmFrame(
	    pmUtility.getResource("SPM:New.Attached.Printer"));
	    helpTag = "InstallLocal";
	    actionName =
	    pmUtility.getResource("New.Attached.Printer");
	    break;
            
	case Constants.ADDNETWORK:

	    Debug.message("CLNT:pmInstPr: ADD NETWORK");
	    frame = new pmFrame(
		pmUtility.getResource("SPM:New.Network.Printer"));
	    helpTag = "InstallNetwork";
	    actionName = pmUtility.getResource("New.Network.Printer");
	    break;

	case Constants.MODIFYATTACHED:

	    Debug.message("CLNT:pmInstPr: MODIFY ATTACHED");
	    frame = new pmFrame(
	    pmUtility.getResource("SPM:Modify.Printer.Properties"));
	    workingPrinter.setPrinterName(myTop.selectedPrinter);

	    failed = false;
	    try {
		    workingPrinter.getPrinterDetails();
	    } catch (Exception e) {
		    failed = true;
		    Debug.message("CLNT:pmInsPr:ModifyAttached caught:" + e);
	    }

	    gatherLogs(workingPrinter);

	    if (failed) {
	    // error popup?
	    // throw something?
	    }
		    
	    pmCalls.debugShowPrinter(workingPrinter);
	    dumpLogs("ModifyAttached()");
		    
	    helpTag = "Modify";
	    // helpTag = "ModifyAttached";
	    
	    actionName = pmUtility.getResource("Modify.Printer.Properties");
	    break;

	case Constants.MODIFYNETWORK:

	    Debug.message("CLNT:pmInstPr: MODIFY NETWORK");
	    frame = new pmFrame(
		pmUtility.getResource("SPM:Modify.Printer.Properties"));
	    workingPrinter.setPrinterName(myTop.selectedPrinter);

	    workingPrinter.setPrinterName(myTop.selectedPrinter);
	    failed = false;

	    try {
	    workingPrinter.getPrinterDetails();
	    } catch (Exception e) {
		failed = true;
		Debug.message("CLNT:pmInstPr:ModifyNetwork caught " + e);
	    }   

	    gatherLogs(workingPrinter);

	    if (failed) {
	    // error popup?
		// throw something?
	    }
	    
	    pmCalls.debugShowPrinter(workingPrinter);
	    dumpLogs("ModifyNetwork()");

	    helpTag = "Modify";
	    // helpTag = "ModifyNetwork";

	    actionName = pmUtility.getResource("Modify.Printer.Properties");
	    
	    break;

		case Constants.MODIFYREMOTE:
	    Debug.message("CLNT:pmInstPr: MODIFY REMOTE");
	    frame = new pmFrame(
		pmUtility.getResource("SPM:Modify.Printer.Properties"));

	    workingPrinter.setPrinterName(myTop.selectedPrinter);

	    failed = false;
	    try {
		   workingPrinter.getPrinterDetails();
	    } catch (Exception e) {
		failed = true;
		Debug.warning("CLNT:pmInstPr:ModifyRemote caught " + e);
	    }
	    gatherLogs(workingPrinter);

	    if (failed) {
		// error popup?
		// throw something?
	    }
	    
	    helpTag = "Modify";
	    // helpTag = "ModifyRemote";

	    actionName = pmUtility.getResource("Modify.Printer.Properties");
	    break;
       
        }
	
	// ensure that pmButton hashtable gets cleaned up
	frame.setClearButtonsOnClose(true);

        setLayout(new BorderLayout());

	// Build the Screen
	northPanel();
	if (action != Constants.MODIFYREMOTE) {
		centerPanel();
	}

	southPanel();

        // default button is always OK, for now...
        // frame.getRootPane().setDefaultButton (okButton);
        // okButton.setAsDefaultButton ();

        // handle Esc as cancel
        this.registerKeyboardAction(new ActionListener() {
            public void actionPerformed(ActionEvent e) {
                Debug.message("CLNT:  default cancel action");
                actioncancelButton();
            }},
            KeyStroke.getKeyStroke(KeyEvent.VK_ESCAPE, 0, false),
            JComponent.WHEN_IN_FOCUSED_WINDOW);
                                                            
	if (action == Constants.ADDLOCAL || action == Constants.ADDNETWORK) {
			defaultComponent = pnameText;
	} else {
              	defaultComponent = descText; 
	}

	frame.setDefaultComponent(defaultComponent);

	defaultComponent.addFocusListener(new FocusListener() {
		public void focusGained(FocusEvent e) {
		    Debug.info("\ncomponent focus gained: " + e);
		}
		public void focusLost(FocusEvent e) {
		    Debug.info("\ncomponent focus lost: " + e);
		}
	});

        /*
		Component xxx;
		if ((xxx = frame.getGlassPane()) != null)
			xxx.addFocusListener(new FocusListener() {
			public void focusGained(FocusEvent e) {
			    Debug.info("\nglass pane focus gained: " + e);
			    transferFocus();
			}
			public void focusLost(FocusEvent e) {
			    Debug.info("\nglass pane focus lost: " + e);
			}
		});
		if ((xxx = this.getRootPane()) != null)
			xxx.addFocusListener(new FocusListener() {
			public void focusGained(FocusEvent e) {
			    Debug.info("\nroot pane focus gained: " + e);
			    transferFocus();
			}
			public void focusLost(FocusEvent e) {
			    Debug.info("\nroot pane focus lost: " + e);
			}
		});
		if ((xxx = frame.getContentPane()) != null)
		xxx.addFocusListener(new FocusListener() {
			public void focusGained(FocusEvent e) {
			    Debug.info("\ncontent pane focus gained: " + e);
			    transferFocus();
			}
			public void focusLost(FocusEvent e) {
			    Debug.info("\ncontent pane focus lost: " + e);
			}
		});
        */

    }


    public class accessListModel extends AbstractListModel {
	int numColumns;
	Vector data;
	Vector access = new Vector(1, 1);

	public accessListModel() {
		numColumns = getColumnCount();
	}
	
	public void addaccessList(String data[]) {

		for (int i = 0; i < data.length; i++) {
			access.addElement(data[i]);
			
		}
	}

	public void addaccessList(String data) {
		access.addElement(data);
	}

	public boolean isduplicate(String d) {
		if (access == null)
			return false;
		else
			return access.contains(d);
	}

	public void removeRow(int row) {
		access.removeElementAt(row);
	}

	public void removeListEntries() {
		access.removeAllElements();
	}

	public int getRowCount() {
		return access.size();
	}

	public int getSize() {
		return access.size();
	}

	public int getColumnCount() {
		return 1;
	}

	public void addRow(Vector row) {
		access.addElement(row);
	}

	public Object getValueAt(int row) {
		return access.elementAt(row);
	}

	public Object getElementAt(int row) {
		return access.elementAt(row);
	}

	public Object getValueAt(int row, int col) {
		return access.elementAt(row);
	}

	public void setValueAt(String value, int row) {
		access.setElementAt(value, row);
	}

	public Vector getAccessList() {
		return access;
	}

	public void accesstoArray(String[] target) {
		access.copyInto(target);
	}

    }

    public void centerPanel() {
	JPanel center = new JPanel();
		
	accessModel = new accessListModel();
	accessList = new JList(accessModel);
	accessList.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);

		
	center.setLayout(new GridBagLayout());
	GridBagConstraints c = new GridBagConstraints();

	ListSelectionModel rowSelectModel = accessList.getSelectionModel();
	rowSelectModel.addListSelectionListener(new ListSelectionListener() {
		public void valueChanged(ListSelectionEvent e) {
			ListSelectionModel accessSM = 
				(ListSelectionModel)e.getSource();
		}
	});

	c.insets = new Insets(5, 5, 5, 5);

	// Create the label
	c.gridx = 0;
	c.gridy = 0;
	labelConstraints(c);
	center.add(new JLabel
		(pmUtility.getResource("User.Access.List:")), c);

	// Create User Access List
	String[] tmp = workingPrinter.getUserAllowList();


	if (tmp == null) {
		accessModel.addaccessList("all");
	} else {
		accessModel.addaccessList(tmp);
	}

	ascrollPane = new JScrollPane(accessList);

	c.gridwidth = 2;
	c.gridx = 1;
	c.weightx = c.weighty = 1.0;
	c.fill = GridBagConstraints.BOTH;
	center.add(ascrollPane, c);

	// Create Textfield
	c.gridx = 1;	
	c.gridy = 1;	
	c.ipadx = 15;
	c.fill = GridBagConstraints.HORIZONTAL;
	c.anchor = GridBagConstraints.WEST;
	c.weightx = c.weighty = 1.0;
	userText = new pmTextField(25);
	center.add(userText, c);

	// Add the Add/Delete Buttons
	c.gridx = 1;
	c.gridy = 2;

	adButtons(c);
	
	c.gridwidth = 1;
	center.add(addButton, c);

	c.gridx = 2;
	center.add(deleteButton, c);

	add("Center", center);

    }

    public void northPanel() {
	JPanel north = new JPanel();
	north.setLayout(new GridBagLayout());
	GridBagConstraints c = new GridBagConstraints();

	northPanelConstraints(c);
	
	// Define the constraints and create the labels

	// All Add/Modify
	labelConstraints(c);
	c.gridx = 0;
	c.gridy = 0;
	printernameLabel(north, c);
	c.gridy++;
	servernameLabel(north, c);
	c.gridy++;
	descriptionLabel(north, c);
	c.gridy++;

	// Add printers, modify local
	if (action != Constants.MODIFYREMOTE) {

		if ((action == Constants.ADDLOCAL) ||
		   (action == Constants.MODIFYATTACHED)) {
			printerportLabel(north, c);
			c.gridy++;
		}

		printertypeLabel(north, c);
		c.gridy++;
		filecontentsLabel(north, c);
		c.gridy++;
		faultnotLabel(north, c);
		c.gridy++;

		if (action == Constants.ADDNETWORK ||
		    action == Constants.MODIFYNETWORK) {
			destinationLabel(north, c);
			c.gridy++;
			protocolLabel(north, c);
			c.gridy++;
		}

		optionsLabel(north, c);
	} else {
		optionLabel(north, c);
	}

	// Define the constraints and create the fields

	// Add printers
	c.gridx = 1;
	c.gridy = 0;

	// Printer Name and Server Name
	if ((action == Constants.ADDLOCAL) || 
		(action == Constants.ADDNETWORK)) {

		TextFieldConstraints(c);
		printernameTextField(north, c);
		c.gridy++;
		labelConstraints(c);
		try {
			north.add(new JLabel(
				myTop.host.getLocalHostName()), c);
		} catch (Exception e) {
			Debug.warning(
			"CLNT:pmInstPr:getLocalHostName exception " + e);
			north.add(new JLabel(" "), c);
		}
		c.gridy++;
	} else {
		labelConstraints(c);
		north.add(new JLabel(myTop.selectedPrinter), c);
		c.gridy++;
		labelConstraints(c);
		north.add(new JLabel(myTop.selprinterServer), c);
		c.gridy++;
	}

	// Description
	TextFieldConstraints(c);
	descriptionField(north, c);
	if (action == Constants.MODIFYATTACHED || 
		action == Constants.MODIFYNETWORK ||
		action == Constants.MODIFYREMOTE) {
		if (workingPrinter.getComment() != null)
			descText.setText(workingPrinter.getComment());
	}
	c.gridy++;

	if (action != Constants.MODIFYREMOTE) {
		if (action == Constants.ADDLOCAL) {
			comboboxConstraints(c);
			printerportField(north, c);
			c.gridy++;

		} else if (action == Constants.MODIFYATTACHED) {
			labelConstraints(c);
			north.add(new JLabel(workingPrinter.getDevice()), c);
			c.gridy++;
		}

		comboboxConstraints(c);
		printertypeField(north, c);
		if (action == Constants.MODIFYATTACHED || 
		    action == Constants.MODIFYNETWORK) {

			setPrinterType();
		}

		c.gridy++;

		filecontentsField(north, c);
		if (action == Constants.MODIFYATTACHED ||
		    action == Constants.MODIFYNETWORK) {
			setType();
		}
		c.gridy++;

		faultnotField(north, c);
		if (action == Constants.MODIFYATTACHED ||
		    action == Constants.MODIFYNETWORK) {

			setFault();
		}
		c.gridy++;

		if (action == Constants.ADDNETWORK ||
		    action == Constants.MODIFYNETWORK) {

			TextFieldConstraints(c);
			destinationField(north, c);
			c.gridy++;
			comboboxConstraints(c);
			protocolField(north, c);
			if (action == Constants.MODIFYNETWORK)
				setNetworkInfo();
			c.gridy++;
		}

		optionsConstraints(c);
		optionsFields(north, c);
		if (action == Constants.MODIFYATTACHED ||
		    action == Constants.MODIFYNETWORK) {
			if (workingPrinter.getIsDefaultPrinter()) {
				defaultp.doClick();
			}
			if (workingPrinter.getBanner()) {
				banner.doClick();
			}
		}
	} else {
		optionsConstraints(c);
		defaultoptionField(north, c);
		if (workingPrinter.getIsDefaultPrinter()) {
			defaultp.doClick();
		}
	}
	
	add("North", north);
    }

    public void setNetworkInfo() {
	String p = workingPrinter.getProtocol();

	if (p != null)  {
		if (p.equals("bsd"))
			protocolCombo.setSelectedItem("BSD");
		else if (p.equals("tcp"))
			protocolCombo.setSelectedItem("TCP");
	}

	if (destText != null)
		destText.setText(workingPrinter.getDestination());
    }

    public void setFault() {
	String fault = workingPrinter.getNotify();

	if (fault == null || fault == "none") 
		faultCombo.setSelectedItem(
		    pmUtility.getResource("None"));

	else if (fault.equals("write"))
		faultCombo.setSelectedItem(
		    pmUtility.getResource("Write.to.Superuser"));

	else if (fault.equals("mail"))
		faultCombo.setSelectedItem(
		    pmUtility.getResource("Mail.to.Superuser"));
	else {
		faultCombo.addItem(fault);
		faultCombo.setSelectedItem(fault);
	}
    }

    public void setType() {
    // lpadmin can combine things like: simple,any ...

	String filedata[] = workingPrinter.getFileContents();
	String filecontents = new String();

	if (filedata == null) {
		fileCombo.setSelectedItem(
			pmUtility.getResource("None"));
	} else {
		for (int i = 0; i < filedata.length; i++) {
			if (i > 0 && filedata[i] != null)
				filecontents = filecontents.concat(",");

			filecontents = filecontents.concat(filedata[i]);
		}
		Debug.message("CLNT:pmInstPr:setType: filecontents = " + 
				filecontents);

		if (filecontents.equals("postscript")) {
		    fileCombo.setSelectedItem(pmUtility.getResource(
								"PostScript"));
		} else if (filecontents.equals("simple")) {
		    fileCombo.setSelectedItem(pmUtility.getResource("ASCII"));
		} else if (filecontents.equals("postscript,simple")) {
		    fileCombo.setSelectedItem(pmUtility.getResource(
						"Both.PostScript.and.ASCII"));
		} else if (filecontents.equals("simple,postscript")) {
			fileCombo.setSelectedItem(pmUtility.getResource(
						"Both.PostScript.and.ASCII"));
		} else if (filecontents.equals("none")) {
			fileCombo.setSelectedItem(
				pmUtility.getResource("None"));
		} else if (filecontents.equals("any")) {
			fileCombo.setSelectedItem(
				pmUtility.getResource("Any"));
		} else {
			Debug.message(
                   "CLNT:pmInstPr:setType()unknown file contents type");
			fileCombo.addItem(filecontents);
			fileCombo.setSelectedItem(filecontents);
		}
	}
    }

    public void setPrinterType() {
	int x = 0;
	String type = workingPrinter.getPrinterType();
	Debug.message(
		"CLNT:pmInstPrsetPrinterType(): printer type is " + type);

       	if (type == null)
       		return;
        
	if (type.equals("PS")) {
		typeCombo.setSelectedItem("PostScript");
	} else if (type.equals("hplaser")) {
		typeCombo.setSelectedItem("HP Printer");
	} else if (type.equals("PSR")) {
		typeCombo.setSelectedItem("Reverse PostScript");
	} else if (type.equals("epson2500")) {
		typeCombo.setSelectedItem("Epson 2500");
	} else if (type.equals("ibmproprinter")) {
		typeCombo.setSelectedItem("IBM ProPrinter");
	} else if (type.equals("qume5")) {
		typeCombo.setSelectedItem("Qume Sprint 5");
	} else if (type.equals("daisy")) {
		typeCombo.setSelectedItem("Daisy");
	} else if (type.equals("diablo")) {
		typeCombo.setSelectedItem("Diablo");
	} else if (type.equals("datagraphix")) {
		typeCombo.setSelectedItem("Datagraphix");
	} else if (type.equals("la100")) {
		typeCombo.setSelectedItem("DEC LA100");
	} else if (type.equals("ln03")) {
		typeCombo.setSelectedItem("DEC LN03");
	} else if (type.equals("decwriter")) {
		typeCombo.setSelectedItem("Dec Writer");
	} else if (type.equals("ti800")) {
		typeCombo.setSelectedItem("Texas Instruments 800");
	} else {
		typeCombo.addItem(type);
		typeCombo.setSelectedItem(type);
		Debug.message(
			"CLNT:pmInstPr:setPrinterType(): user defined type " +
			 type);
	}

    }

    public void getCommon() {

	ptype = (String)typeCombo.getSelectedItem();
	fcontents = (String)fileCombo.getSelectedItem();
	faultn = (String)faultCombo.getSelectedItem();

	// user access list
	Debug.message(
	    "CLNT:pmInstPr:getCommon(): printer type : " + ptype);
	Debug.message(
	    "CLNT:pmInstPr:getCommon(): file contents : " + fcontents);
	Debug.message(
	    "CLNT:pmInstPr:getCommon(): fault notification : " + faultn);
	Debug.message(
	    "CLNT:pmInstPr:getCommon(): default printer : " +
		defaultp.isSelected());
	Debug.message(
	    "CLNT:pmInstPr:getCommon(): banner : " + banner.isSelected());
    }

    public void getPrinterServer() throws pmGuiException {

	// Printer Name is a required field.
	printer = pnameText.getText();
	if (printer.equals("")) {
		pnameText.requestFocus();
		throw new pmIncompleteFormException(
			pmUtility.getResource("Printer.name.required."));
	}

	if (!Valid.localPrinterName(printer)) {
		pnameText.requestFocus();
		throw new pmIncompleteFormException(
			pmUtility.getResource("Printer.name.invalid."));
	}

	server = null;
	try {
		server = myTop.host.getLocalHostName();
	} catch (Exception e) {
		Debug.warning(
		"CLNT:pmInstPr:getLocalHostName exception " + e);
	}
		
	Debug.message(
	    "CLNT:pmInstPr:getPrinterServer(): printer is: " + printer);
	Debug.message(
	    "CLNT:pmInstPr:getPrinterServer(): server is: " + server);
	
    }

    public void getPort() {
	port = (String)portCombo.getSelectedItem();
	Debug.message("CLNT:pmInstPr:getPort(): port is: " + port);
    }

    public void getNetworkInfo() throws pmIncompleteFormException {
	destination = destText.getText();
	if (destination.equals("")) {
		destText.requestFocus();
		throw new pmIncompleteFormException(
			pmUtility.getResource("Destination.required."));
	}

	if (!Valid.destination(destination)) {
		destText.requestFocus();
		throw new pmIncompleteFormException(
			pmUtility.getResource("Destination.invalid."));
	}

	protocol =  (String)protocolCombo.getSelectedItem();
	port = new String("/dev/null");
	Debug.message(
		"CLNT:pmInstPr:getNetworkInfo(): destination is: " +
		destination);
	Debug.message(
	    "CLNT:pmInstPr:getNetworkInfo(): protocol is: " + protocol);
    }

    public void getDescription() {

	String messy;

	messy = descText.getText();
	description = messy.trim();
	Debug.message(
		"CLNT:pmInstPr:getDescription():description: " + "<" +
			description + ">");

	if (workingPrinter.getComment() == null && description.equals(""))
		description = null;

    }

    public void getUserAccess() {
		
	if (accessModel.getRowCount() != 0) {
		useraccesslist = new String[accessModel.getRowCount()];
		accessModel.accesstoArray(useraccesslist);
	} else {
		useraccesslist = null;
	}
    }

    public String gui2lpptype(String t) {
	String lp;

	if (t != null) {
	    if (t.equals("PostScript"))
		lp = new String("PS");
	    else if (t.equals("HP Printer"))
			lp = new String("hplaser");
	    else if (t.equals("Reverse PostScript"))
			lp = new String("PSR");
	    else if (t.equals("Epson 2500"))
			lp = new String("epson2500");
	    else if (t.equals("IBM ProPrinter"))
			lp = new String("ibmproprinter");
	    else if (t.equals("Qume Sprint 5"))
			lp = new String("qume5");
	    else if (t.equals("Daisy"))
			lp = new String("daisy");
	    else if (t.equals("Diablo"))
			lp = new String("diablo");
	    else if (t.equals("Datagraphix"))
			lp = new String("datagraphix");
	    else if (t.equals("DEC LA100"))
			lp = new String("la100");
	    else if (t.equals("DEC LN03"))
			lp = new String("ln03");
	    else if (t.equals("Dec Writer"))
			lp = new String("decwriter");
	    else if (t.equals("Texas Instruments 800"))
			lp = new String("ti800");
	    else {
		Debug.message(
			"CLNT:pmIns:gui2lptype: printer type unknown: " + t);
		lp = new String(t);
	    }
	} else {
	    Debug.message("CLNT:pmInstPr:gui2lptype: input printer type null");
	    lp = new String("");
	}

	return lp;
    }

    public String[] gui2lpfcontents(String f) {

	String[] lp = null;

	if (f != null) {
		if (f.equals(pmUtility.getResource("PostScript"))) {
			lp = new String[1];
			lp[0] = new String("postscript");
		} else if (f.equals(pmUtility.getResource("ASCII"))) {
			lp = new String[1];
			lp[0] = new String("simple");
		} else if (f.equals(pmUtility.getResource(
				"Both.PostScript.and.ASCII"))) {
			lp = new String[2];
			lp[0] = new String("simple");
			lp[1] = new String("postscript");
		} else if (f.equals(pmUtility.getResource("Any"))) {
			lp = new String[1];
			lp[0] = new String("any");
		} else if (f.equals(pmUtility.getResource("None"))) {
			lp = new String[1];
			lp[0] = new String("none");
		}
	} else {
	    Debug.message(
		"CLNT:pmInstPr:gui2lpfcontents(): input string null");
	}

	return lp;
    }

    public String gui2lpfaultn(String n) {
	String lp = null;
	if (n != null) {
		if (n.equals(pmUtility.getResource("Write.to.Superuser")))
			lp = new String("write");
		else if (n.equals(pmUtility.getResource("Mail.to.Superuser")))
			lp = new String("mail");
		else if (n.equals(pmUtility.getResource("None")))
			lp = new String("none");
			
	} else {
		Debug.message(
		"CLNT:pmInstPr:gui2lpfaultn():input faultnotify null");
	}
	return lp;
    }

    public String gui2lpprotocol(String p) {
	String lp = null;
	if (p.equals("TCP"))
		lp = new String("tcp");
	else if (p.equals("BSD"))
		lp = new String("bsd");
	else {
		Debug.message(
		"CLNT:pmInstPr:gui2lpprotocol: protocol is empty");
	}
	return lp;
    }

    public void updatePrinter() {
        if (workingPrinter != null) {
            if (printer != null)
                workingPrinter.setPrinterName(printer);
            if (server != null)
                workingPrinter.setPrintServer(server);
            if (description != null)
                workingPrinter.setComment(description);
            if (port != null)
                workingPrinter.setDevice(port);
            if (ptype != null)
                workingPrinter.setPrinterType(gui2lpptype(ptype));
            if (fcontents != null)
                workingPrinter.setFileContents(gui2lpfcontents(fcontents));
            if (faultn != null)
                workingPrinter.setNotify(gui2lpfaultn(faultn));
            if (destination != null)
                workingPrinter.setDestination(destination);
            if (protocol != null)
                workingPrinter.setProtocol(gui2lpprotocol(protocol));
            if (useraccesslist != null)
                workingPrinter.setUserAllowList(useraccesslist);
            else {
                String[] a = new String[1];
                a[0] = new String("none");
                workingPrinter.setUserAllowList(a);
            }
                
            if (defaultp != null)
                workingPrinter.setIsDefaultPrinter(defaultp.isSelected());
                
            if (banner != null)
                workingPrinter.setBanner(banner.isSelected());

	} else {
	    Debug.warning(
                "CLNT:pmInstPr:updatePrinter(): workingPrinter null");
	}
    }


    void gatherLogs(Printer p) {
        cmdLog = p.getCmdLog();
        errorLog = p.getErrorLog();
        warnLog = p.getWarnLog();
    }


    void dumpLogs(String who) {
        Debug.message(who);
        Debug.message(who + " command: " + cmdLog);
        Debug.message(who + " warnings: " + warnLog);
        Debug.message(who + " errors: " + errorLog);
    }
    

    public void doAddLocal() throws pmGuiException {

        try {
            getPrinterServer();
            getDescription();
            getPort();
            getCommon();
            getUserAccess();
            updatePrinter();

        } catch (pmIncompleteFormException ie) {
            throw new pmIncompleteFormException(ie.getMessage());
        }


        boolean exist;
        boolean failed = false;

        // exists could throw an exception from the underyling cmds...
        try {
		exist = PrinterUtil.exists(printer, myTop.ns);
        } catch (Exception e) {
            throw new pmGuiException();
        } 

        if (exist) {
            throw new pmPrinterExistsException();
        }

        try {
            workingPrinter.addLocalPrinter();
        } catch (Exception e) {
            failed = true;
        } finally {
        	gatherLogs(workingPrinter);
            pmCalls.debugShowPrinter(workingPrinter);
            dumpLogs("doAddLocal()");
            if (failed)
                throw new pmAddPrinterFailedException(errorLog);
        }

    }

    
    public void doAddNetwork() throws pmGuiException {
	try {
		getPrinterServer();
		getDescription();
		getNetworkInfo();
		getCommon();
		getUserAccess();
		updatePrinter();
	} catch (pmIncompleteFormException ie) {
		throw new pmIncompleteFormException(ie.getMessage());
	}

       	boolean exist = false;
        
	try {
		exist = PrinterUtil.exists(printer, myTop.ns);
	} catch (Exception e) {
		Debug.message(
		"CLNT:pmInstPr:doAddNetwork:printer exists " + e);
           		throw new pmGuiException();
	}

        if (exist) {
            Debug.message(
		"CLNT:pmInstPr:Trying to add existing printer: "   + printer);
            throw new pmPrinterExistsException();
        }

        boolean failed = false;
        try {
            workingPrinter.addLocalPrinter();
        } catch (Exception e) {
            failed = true;
        } finally {
            gatherLogs(workingPrinter);
            pmCalls.debugShowPrinter(workingPrinter);
            dumpLogs("doAddNetwork()");
            if (failed)
                throw new pmAddPrinterFailedException(errorLog);
        }

    }

        
    public void doModifyLocalAttached() throws pmGuiException {
	getDescription();
	getCommon();
	getUserAccess();
	updatePrinter();

       	boolean failed = false;
       	try {
       		workingPrinter.modifyPrinter();
       	} catch (Exception e) {
       		Debug.warning("CLNT:doModifyLocalAttached: " + e);
       		failed = true;
       	} finally {
       		gatherLogs(workingPrinter);
       		pmCalls.debugShowPrinter(workingPrinter);
       		dumpLogs("doModifyLocalAttached()");
	if (failed)
               	throw new pmModifyPrinterFailedException(errorLog);
       	}

    }

    public void doModifyLocalNetwork() throws pmGuiException {
	getDescription();
	getNetworkInfo();
	getCommon();
	getUserAccess();
	updatePrinter();

       	boolean failed = false;
	try {
           	workingPrinter.modifyPrinter();
	} catch (Exception e) {
		Debug.warning("CLNT:pmInstPr:doModifyLocalAttached: " + e);
		failed = true;
	} finally {
       		gatherLogs(workingPrinter);
		pmCalls.debugShowPrinter(workingPrinter);
		dumpLogs("doModifyLocalNetwork()");
       		if (failed)
               		throw new pmModifyPrinterFailedException(errorLog);
	}

    }


    public void doModifyRemote() throws pmGuiException {
	getDescription();
	updatePrinter();

        boolean failed = false;
	try {
		workingPrinter.modifyPrinter();
	} catch (Exception e) {
		Debug.warning("CLNT:doModifyRemote: " + e);
		failed = true;
	} finally {
       		gatherLogs(workingPrinter);
		pmCalls.debugShowPrinter(workingPrinter);
		dumpLogs("doModifyRemote()");
       		if (failed)
               		throw new pmModifyPrinterFailedException(errorLog);
	}

    }

    
    public void doClearFields() {

	if (pnameText != null)
		pnameText.setText("");	
	if (snameText != null)
		snameText.setText("");	
	if (descText != null)
		descText.setText("");	
	if (portCombo != null)
		portCombo.setSelectedIndex(0);
	if (typeCombo != null)
		typeCombo.setSelectedIndex(0);
	if (fileCombo != null)
		fileCombo.setSelectedIndex(0);
	if (faultCombo != null)
		faultCombo.setSelectedIndex(0);
	if (protocolCombo != null)
		protocolCombo.setSelectedIndex(0);
	if (destText != null)
		destText.setText("");	

	if (defaultp.isSelected())
		defaultp.doClick();

	if (banner.isSelected())
		banner.doClick();

	accessModel.removeListEntries();
	accessModel.addaccessList("all");
    }

    public void doResetFields() {

	if (workingPrinter != null) {
	    try {
		if (workingPrinter.getComment() != null)
		    descText.setText(workingPrinter.getComment());
		else
		    descText.setText("");
	    } catch (Exception e) {
		Debug.message(
			"CLNT:pmInstallPr:doResetFields(): getComment() " +
			"Exception: " + e);
	    }

	    if (action == Constants.ADDLOCAL ||
		action == Constants.ADDNETWORK ||
		action == Constants.MODIFYATTACHED) {
		try {
			portCombo.setSelectedItem(workingPrinter.getDevice());
		} catch (Exception e) {
			Debug.message(
				"CLNT:pmInsPr:doResetFields(): getDevice() " +
				"Exception: " + e);
		}
	    }
		

	    if (action != Constants.MODIFYREMOTE) {
		setPrinterType();
		setType();
		setFault();

		if (action == Constants.MODIFYNETWORK)
			setNetworkInfo();
	
		try {
			accessModel.removeListEntries();
			accessModel.addaccessList(
				workingPrinter.getUserAllowList());
			accessList.setListData(accessModel.getAccessList());
			accessList.ensureIndexIsVisible(0);
		} catch (Exception e) {
			Debug.warning(
			    "CLNT:InstPr:doResetFields(): addaccessList() " +
			    "Exception: " + e);
		} finally {
			accessList.clearSelection();
		}



		// selected and banner object out of sync
		if ((banner.isSelected() && !workingPrinter.getBanner()) ||
			(!banner.isSelected() && workingPrinter.getBanner()))

				banner.doClick();
	    }


	    // selected and printer object out of sync
	    if ((defaultp.isSelected() && 
			!workingPrinter.getIsDefaultPrinter()) ||

		(!defaultp.isSelected() && 
			workingPrinter.getIsDefaultPrinter()))

			defaultp.doClick();

	    }
    }



    public boolean isactionModify() {

	if (action == Constants.MODIFYATTACHED ||
	    action == Constants.MODIFYNETWORK  ||
	    action == Constants.MODIFYREMOTE)

		return true;
	else
		return false;
    }

    public void doReset() {
	Debug.message("CLNT:pmInsPr:doReset()");
	if (action == Constants.ADDLOCAL ||
	    action == Constants.ADDNETWORK) {

		doClearFields();
	} else {
		doResetFields();
	}

        // as a side effect, the OK button will regain default status
        if (defaultComponent != null) 
		defaultComponent.requestFocus();
    }

    public void doAction() throws pmGuiException {

	// if nameservice, check for login

        if (myTop.ns.getNameService().equals("nis") == true) {
            try {
                if (!myTop.ns.isAuth()) {
                    pmUtility.doLogin(myTop, frame);
                }
            } catch (pmUserCancelledException e) {
		throw new pmLoginFailedException(
			pmUtility.getResource("User.cancelled.login."));
            } catch (pmGuiException ge) {
                Debug.message("CLNT:pmInstPr: Required login failed.");
                pmMessageDialog m = new pmMessageDialog(
                    frame,
                    pmUtility.getResource("Error"),
                    pmUtility.getResource("Required.login.failed."),
                    myTop,
                    "LoginFailed");
                m.show();
		Debug.message("CLNT:pmInstPr:required login failed.");
                throw new pmLoginFailedException(
                    pmUtility.getResource("Required.login.failed."));
            } catch (Exception e) {
                Debug.message("CLNT:pmInstPr:login exception: " + e);
                pmMessageDialog m = new pmMessageDialog(
                    frame,
                    pmUtility.getResource("Error"),
                    pmUtility.getResource("Required.login.failed."),
                    myTop,
                    "LoginFailed");
                m.show();
                throw new pmLoginFailedException(
                    pmUtility.getResource("Required.login.failed."));
            }
        }

	frame.setCursor(Cursor.getPredefinedCursor(Cursor.WAIT_CURSOR));

	Debug.message("CLNT:pmInstPr:doAction: action " + action);

	// Check for confirmation option
	if (((myTop.getConfirmOption() == true) && (confirmAction() == true))
                        || (myTop.getConfirmOption() == false)) {

        try {

            switch (action) {

	    case Constants.ADDLOCAL:
                doAddLocal();
		break;
	    case Constants.ADDNETWORK:
                doAddNetwork();
		break;
	    case Constants.MODIFYATTACHED:
		workingPrinter.getPrinterDetails();
		Debug.message(
			"CLNT:pmInstPr:Printer Details: server is " +
               	    	workingPrinter.getPrintServer());
		doModifyLocalAttached();
		break;
	    case Constants.MODIFYNETWORK:
		// add dest and protocol
		workingPrinter.getPrinterDetails();
               	Debug.message("CLNT:pmInstPr:Printer Details: server is " +
               			workingPrinter.getPrintServer());
               	doModifyLocalNetwork();
		break;
	    case Constants.MODIFYREMOTE:
		workingPrinter.getPrinterDetails();
               	doModifyRemote();
		break;
	    }

	} catch (pmIncompleteFormException fe) {
		pmMessageDialog m = new pmMessageDialog(
		    frame,
		    pmUtility.getResource("Error"),
		    fe.getMessage());	// "FormError"
		m.show();
		throw new pmIncompleteFormException();
        } catch (pmPrinterExistsException ee) {
            	pmMessageDialog m = new pmMessageDialog(
		    frame, 
		    pmUtility.getResource("Error"),
		    pmUtility.getResource(
			"The.specified.printer.already.exists."));
            	m.show();
	} catch (pmNullSelectedPrinterException ne) {
        	pmMessageDialog m = new pmMessageDialog(
		    frame, 
		    pmUtility.getResource("Error"),
		    pmUtility.getResource(
			"The.selected.printer.does.not.exist."));
            	m.show();
                cleanup();
                // frame.dispose();
        } catch (pmAddPrinterFailedException ae) {
         	pmMessageDialog m = new pmMessageDialog(
		    frame, 
		    pmUtility.getResource("Error"),
		    ae.getMessage(),
		    myTop,
		    "AddPrinterFailed");  
		m.show();
            
        } catch (pmModifyPrinterFailedException me) {
            	pmMessageDialog m = new pmMessageDialog(
		    frame, 
		    pmUtility.getResource("Error"),
		    me.getMessage(),
		    myTop,
		    "ModifyFailed");  
            	m.show();
        } catch (pmGuiException ge) {
            	pmMessageDialog m = new pmMessageDialog(
		    frame, 
		    pmUtility.getResource("Application.Error"),
		    ge.toString());
            	m.show();

        } catch (pmCmdFailedException cfe) {
		String msg = cfe.getMessage();
            	if (msg == null || msg.length() == 0) 
			msg = pmUtility.getResource(
                                "error.message.command-failed");
		
		pmMessageDialog m = new pmMessageDialog(
		    frame, 
		    pmUtility.getResource("Command.Failed.Error"),
		    msg);
            	m.show();

        } catch (Exception e) {
            	pmMessageDialog m = new pmMessageDialog(
		    frame, 
		    pmUtility.getResource("Unknown.Application.Error"),
		    e.toString());
            	m.show();

        } finally {
		frame.setCursor(Cursor.getDefaultCursor());
            	myTop.setLogData(cmdLog, errorLog, warnLog);
            	myTop.showLogData(actionName);
        }

	// Update the list of printers
	myTop.pmsetPrinterList();
	}

    }

    public boolean confirmAction() {
	if (myTop.getConfirmOption() == true) {
		pmOKCancelDialog d = new pmOKCancelDialog(frame,
			    pmUtility.getResource("Action.Confirmation"),
			    pmUtility.getResource(
					"Continue.action.for.this.printer?"));
		d.show();
		if (d.getValue() != JOptionPane.OK_OPTION) {
			pmMessageDialog m =
				new pmMessageDialog(
				frame,
				pmUtility.getResource("Warning"),
				pmUtility.getResource("Operation.Cancelled"));
			m.show();
			return false;
		}
	}
	return true;
    }


    public void actionportCombo() {
	Debug.message("CLNT:pmInstPr:actionportCombo()");
	getPort();
	if (port.equals(pmUtility.getResource("Other..."))) {
	    pmOther o = new pmOther(
		frame,
		pmUtility.getResource("SPM:Specify.Printer.Port"),
		pmUtility.getResource("Enter.printer.port.or.file"),
		myTop,
		"PrinterPort");

	    o.show();
	    if (o.getValue() == JOptionPane.OK_OPTION) {
		    port = o.deviceName.getText();
		   int idx = portCombo.getItemCount(); 
		    try {
			if (!port.equals("") && Valid.device(port)) {
				portCombo.insertItemAt(
					port, (idx > 0) ? idx -1 : idx);
				portCombo.setSelectedItem(port);
			} else if (!port.equals("")) {
			    pmMessageDialog m = new pmMessageDialog(
				frame,
				pmUtility.getResource("Error"),
				pmUtility.getResource(
					"Device.missing.or.not.writeable."),
				myTop,
				"PrinterPort");	// "InvalidDevice"
			    m.show();
			    if (olddevice == null)
				    portCombo.setSelectedIndex(0);
			    else
				    portCombo.setSelectedItem(olddevice);
			} else {
			    if (olddevice == null)
				portCombo.setSelectedIndex(0);
			    else
				portCombo.setSelectedItem(olddevice);
			}
		    } catch (Exception de) {
			Debug.warning(
			    "CLNT:pmInstPr:error validating device" + de);
		    }

	    } else {		// Cancelled out of Other window
		if (olddevice == null)
		    portCombo.setSelectedIndex(0);
		else
		    portCombo.setSelectedItem(olddevice);
	    }
	}
	olddevice = (String)portCombo.getSelectedItem();
    }

    public void actiontypeCombo() {

	Debug.message("CLNT:pmInstPr:actiontypeCombo()");
	ptype = (String)typeCombo.getSelectedItem();

	if (ptype.equals(pmUtility.getResource("Other..."))) {
		pmOther o = new pmOther(
			frame,
			pmUtility.getResource("SPM:Specify.Printer.Type"),
			pmUtility.getResource("Enter.printer.type:"),
			myTop,
			"PrinterType");
		o.show();
		if (o.getValue() == JOptionPane.OK_OPTION) {
			ptype = o.deviceName.getText();
			int idx = typeCombo.getItemCount();
			try {
			    if (!ptype.equals("") && Valid.printerType(ptype)) {
				typeCombo.insertItemAt(
					ptype, (idx > 0) ? idx -1 : idx);
				typeCombo.setSelectedItem(ptype);
			    } else if (!ptype.equals("")) {
				pmMessageDialog m = new pmMessageDialog(
				    frame,
				    pmUtility.getResource("Error"),
                           	    pmUtility.getResource(
					"Invalid.printer.type."),
				    myTop,
				    "PrinterType");
				m.show();
				if (oldptype == null)
					typeCombo.setSelectedIndex(0);
				else
					typeCombo.setSelectedItem(oldptype);
			    } else {
				if (oldptype == null)
					typeCombo.setSelectedIndex(0);
				else
					typeCombo.setSelectedItem(oldptype);
			    }

			} catch (Exception pte) {
			    Debug.message(
				"CLNT:pmInstPr:error validating printertype"
				+ pte);
			}

		} else {	// Cancelled out of Other window
			if (oldptype == null)
				typeCombo.setSelectedIndex(0);
			else
				typeCombo.setSelectedItem(oldptype);
		}
	}
	oldptype = (String)typeCombo.getSelectedItem();
    }

    public void actionaddButton() {

	String tmp = null;
	String trimtmp;
	Debug.message("CLNT:pmInstPr:actionaddButton()");
	try {
		tmp = userText.getText();
	} catch (Exception e) {
		Debug.message(
			"CLNT:pmInstPr:actionaddButton:UserAccessList empty");
	}

	if (tmp == null || tmp.equals("") || tmp.trim().equals("")) {
		Debug.message("CLNT:pmInstPr:no username to add");
	} else {
		trimtmp = tmp.trim();
		if (accessModel.isduplicate(trimtmp))
		    Debug.message("CLNT:pmInstPr:duplicate user");

		else if (!Valid.user(trimtmp)) {
		    pmMessageDialog m = new pmMessageDialog(
			   frame,
			   pmUtility.getResource("Error"),
			   pmUtility.getResource("Invalid.username"));
			   // "FormError"
		    m.show();

		} else {
		    if (accessModel.getRowCount() > 0 && 
		    (accessModel.getElementAt(0).equals("all") ||
		     accessModel.getElementAt(0).equals("none")))
				    accessModel.removeRow(0);

		    if (trimtmp.equals("all") || 
			    trimtmp.equals("none")) {
			    accessModel.removeListEntries();
		    }

		    accessModel.addaccessList(trimtmp);
		    accessList.setListData(accessModel.getAccessList());
		    int rw = accessModel.getRowCount();
		    accessList.setSelectedIndex(rw - 1);
		    accessList.ensureIndexIsVisible(rw -1);
		}

	}
	userText.setText("");
	userText.requestFocus();

	ascrollPane.revalidate();
	ascrollPane.repaint();

	frame.repaint();
    }

    public void actiondeleteButton() {
	Debug.message("CLNT:pmInstPr:actiondeleteButton()");
	int row;
	int rcount;
	int idx;

	row = accessList.getSelectedIndex();
	rcount = accessModel.getRowCount();

	if ((row >=  0 && rcount > 0) && (!accessList.isSelectionEmpty())) {
		accessModel.removeRow(row);
		rcount = accessModel.getRowCount();
		if (rcount != 0) {
			if (row == rcount)
				accessList.setSelectedIndex(row - 1);
		idx = accessList.getFirstVisibleIndex();
		if (idx > 0 && idx < rcount)
			accessList.ensureIndexIsVisible(idx - 1);
		}
	}
	userText.requestFocus();
	frame.repaint();
    }


    // For ok/cancel
    public void cleanup() {

	if (action == Constants.ADDLOCAL) {
		myTop.localinstallView = null;
	}

	if (action == Constants.ADDNETWORK) {
		myTop.networkinstallView = null;
	}

    }

    public void pmScreendispose() {
	frame.dispose();
    }

    public void actionokButton() {

	Debug.message("CLNT:pmInstPr:actionokButton()");
	boolean incomplete = false;

	try {	
		doAction();
	} catch (pmLoginFailedException le) {
		// User already notified
		Debug.message("CLNT:pmInstPr:pmLoginFailedException caught");
	} catch (pmIncompleteFormException fe) {
		// User already notified
		incomplete = true;
	} catch (pmGuiException ge) {
		Debug.message("CLNT:pmInstPr:login Exception, task cancelled");
	}

	if (!incomplete) {
	    	cleanup();
	    	myTop.pmsetdefaultpLabel();
	    	Debug.message("CLNT:pmInstPr:actionokbutton(): work done");
            	pmCalls.debugShowPrinter(workingPrinter);
            	frame.setVisible(false);
            	frame.repaint();
            	// frame.dispose();
            	myTop.scrollPane.revalidate();
            	myTop.scrollPane.repaint();
	}
    }
 
    public void actionapplyButton() {

	Debug.message("CLNT:pmInstPr:actionapplyButton()");

	try {	
            doAction();
	} catch (pmLoginFailedException le) {
		// User already notified
            	Debug.message("CLNT:pmInstPr:pmLoginFailedException caught");
	} catch (pmGuiException ge) {
            	Debug.message("CLNT:pmInstPr:login Exception, task cancelled");
	}

	myTop.pmsetdefaultpLabel();
	Debug.message("CLNT:pmInstPr:actionapplybutton(): work done");
        pmCalls.debugShowPrinter(workingPrinter);
        myTop.scrollPane.revalidate();
        myTop.scrollPane.repaint();
    }
 
 
    public void actionresetButton() {
        Debug.message("CLNT:pmInstPr:actionresetButton()");
        doReset();
        frame.repaint();
    }

    public void actioncancelButton() {
        Debug.message("CLNT:pmInstPr:actioncancelButton()");
        cleanup();
        frame.setVisible(false);
        frame.repaint();
        // frame.dispose();
    }   
 
    public void actionhelpButton() {
        Debug.message("CLNT:pmInstPr:actionhelpButton()");
        myTop.showHelpItem(helpTag);
    }

    public void Show() {

	Debug.message("CLNT:pmInstPr:Show()");
	frame.getContentPane().add("North", this);
	frame.pack();
	frame.setVisible(true);
	frame.repaint();

        // set focus to initial field, depending on which action is tbd
        // this seems to work best after pack()

        // default button is always OK, for now...
        // frame.getRootPane().setDefaultButton (okButton);
        okButton.setAsDefaultButton();


	Debug.info("Show: default comp is " + defaultComponent);
	if (defaultComponent != null) {
		defaultComponent.requestFocus();
	}

    }
}

