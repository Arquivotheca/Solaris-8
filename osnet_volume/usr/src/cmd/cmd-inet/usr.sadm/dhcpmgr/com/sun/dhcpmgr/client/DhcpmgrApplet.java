/*
 * @(#)DhcpmgrApplet.java	1.10	99/11/18 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.client;

import java.awt.*;
import java.awt.event.*;
import java.util.*;
import java.net.*;
import java.rmi.RemoteException;
import java.rmi.NotBoundException;
import javax.swing.*;
import java.text.MessageFormat;
import java.applet.AppletContext;

import com.sun.dhcpmgr.ui.*;
import com.sun.dhcpmgr.data.*;
import com.sun.dhcpmgr.server.DhcpMgr;
import com.sun.dhcpmgr.bridge.BridgeException;

/**
 * Main class for DHCP Manager.  It is theoretically possible to run this
 * application as a command managing the local system, a command managing a
 * remote system using RMI, or as an applet managing the system from which it
 * was downloaded.  We presently only support the first option, but there is
 * vestigial code here from when the other options were supported as they may
 * be again.  That's why we extend JApplet.
 */
public class DhcpmgrApplet extends JApplet {
    private static MainFrame frame = null;
    private JButton button;
    public static boolean modeIsRelay;
    private static HelpIds helpIds = null;
    private static URL docBase = null;
    private static AppletContext appletContext = null;
    private AddressView addressView;
    private RestartAction restartAction;
    private StopAction stopAction;
    private StartAction startAction;
    private DisableAction disableAction;
    private EnableAction enableAction;
    
    // Handler for Help->Overview menu item
    class OverviewAction extends AbstractAction {
	public OverviewAction() {
	    super(ResourceStrings.getString("overview_item"));
	}
	
	public void actionPerformed(ActionEvent e) {
	    showHelp("overview");
	}
    }
    
    // Handler for Help->How To menu item
    class HowToAction extends AbstractAction {
	public HowToAction() {
	    super(ResourceStrings.getString("howto_item"));
	}
	
	public void actionPerformed(ActionEvent e) {
	    showHelp("howto");
	}
    }
    
    // Handler for Help->Index menu item
    class IndexAction extends AbstractAction {
	public IndexAction() {
	    super(ResourceStrings.getString("index_item"));
	}
	
	public void actionPerformed(ActionEvent e) {
	    showHelp("index");
	}
    }
    
    // Handler for Help->On Service menu item
    class ServiceAction extends AbstractAction {
	public ServiceAction() {
	    super(ResourceStrings.getString("on_service_item"));
	}
	
	public void actionPerformed(ActionEvent e) {
	    showHelp("service_reference");
	}
    }
    
    // Handler for the Service->Restart menu item
    class RestartAction extends AbstractAction {
	public RestartAction() {
	    super(ResourceStrings.getString("restart_item"));
	}
	
	public void actionPerformed(ActionEvent e) {
	    try {
		DataManager.get().getDhcpServiceMgr().reload();
		frame.setStatusText(
		    ResourceStrings.getString("service_restarted"));
	    } catch (Throwable t) {
		Object [] args = new Object[1];
		MessageFormat form = new MessageFormat(
		    ResourceStrings.getString("restart_server_error"));
		args[0] = t.getMessage();
		JOptionPane.showMessageDialog(frame, form.format(args),
		    ResourceStrings.getString("server_error_title"),
		    JOptionPane.ERROR_MESSAGE);
	    }
	}
    }
    
    // Handler for the Service->Stop menu item
    class StopAction extends AbstractAction {
	public StopAction() {
	    super(ResourceStrings.getString("stop_item"));
	}
	
	public void actionPerformed(ActionEvent e) {
	    try {
		DataManager.get().getDhcpServiceMgr().shutdown();
		frame.setStatusText(
		    ResourceStrings.getString("service_stopped"));
		startAction.setEnabled(true);
		restartAction.setEnabled(false);
		setEnabled(false);
	    } catch (Throwable t) {
		Object [] args = new Object[1];
		MessageFormat form = new MessageFormat(
		    ResourceStrings.getString("shutdown_server_error"));
		args[0] = t.getMessage();
		JOptionPane.showMessageDialog(frame, form.format(args),
		    ResourceStrings.getString("server_error_title"),
		    JOptionPane.ERROR_MESSAGE);
	    }
	}
    }
    
    // Handler for the Service->Restart menu item
    class StartAction extends AbstractAction {
	public StartAction() {
	    super(ResourceStrings.getString("start_item"));
	}
	
	public void actionPerformed(ActionEvent e) {
	    try {
		DataManager.get().getDhcpServiceMgr().startup();
		frame.setStatusText(
		    ResourceStrings.getString("service_started"));
		stopAction.setEnabled(true);
		restartAction.setEnabled(true);
		setEnabled(false);
	    } catch (Throwable t) {
		Object [] args = new Object[1];
		MessageFormat form = new MessageFormat(
		    ResourceStrings.getString("startup_server_error"));
		args[0] = t.getMessage();
		JOptionPane.showMessageDialog(frame, form.format(args),
		    ResourceStrings.getString("server_error_title"),
		    JOptionPane.ERROR_MESSAGE);
	    }
	}
    }
    
    // handler for the Service->Disable service menu item
    class DisableAction extends AbstractAction {
	public DisableAction() {
	    super(ResourceStrings.getString("disable_item"));
	}
	
	public void actionPerformed(ActionEvent e) {
	    DisableServiceDialog d = new DisableServiceDialog(frame, true);
	    d.addActionListener(new ActionListener() {
		public void actionPerformed(ActionEvent e) {
		    // Update menu item state once we've disabled it
		    enableAction.setEnabled(true);
		    disableAction.setEnabled(false);
		    stopAction.setEnabled(false);
		    startAction.setEnabled(true);
		    restartAction.setEnabled(false);
		}
	    });
	    d.pack();
	    d.show();
	}
    }
    
    // handler for the Service->Enable service menu item
    class EnableAction extends AbstractAction {
	public EnableAction() {
	    super(ResourceStrings.getString("enable_item"));
	}
	
	public void actionPerformed(ActionEvent e) {
	    DisableServiceDialog d = new DisableServiceDialog(frame, false);
	    d.addActionListener(new ActionListener() {
		public void actionPerformed(ActionEvent e) {
		    // Update menu item state once we've enabled it
		    disableAction.setEnabled(true);
		    enableAction.setEnabled(false);
		    stopAction.setEnabled(true);
		    startAction.setEnabled(false);
		    restartAction.setEnabled(true);
		}
	    });
	    d.pack();
	    d.show();
	}
    }
    
    // handler for the Service->Modify service menu item
    class ModifyServiceAction extends AbstractAction {
	public ModifyServiceAction() {
	    super(ResourceStrings.getString("modify_service_item"));
	}
	
	public void actionPerformed(ActionEvent e) {
	    try {
		DhcpdOptions opts =
		    DataManager.get().getDhcpServiceMgr().readDefaults();
		ServerOptionsDialog d = new ServerOptionsDialog(frame, opts);
		d.pack();
		d.show();
	    } catch (BridgeException ex) {
		// Error reading options
		MessageFormat form = new MessageFormat(
		    ResourceStrings.getString("err_reading_options"));
		Object [] args = new Object[] { ex.getMessage() };
		JOptionPane.showMessageDialog(frame, form.format(args),
		    ResourceStrings.getString("server_error_title"),
		    JOptionPane.ERROR_MESSAGE);
	    }
	}
    }
    
    // handler for the Service->Unconfigure service menu item
    class UnconfigureServiceAction extends AbstractAction {
	public UnconfigureServiceAction() {
	    super(ResourceStrings.getString("unconfigure_service_item"));
	}
	
	public void actionPerformed(ActionEvent e) {
	    UnconfigureDialog d = new UnconfigureDialog(frame);
	    d.addActionListener(new ActionListener() {
		public void actionPerformed(ActionEvent e) {
		    if (e.getActionCommand().equals(DialogActions.OK)) {
			/*
			 * User unconfigured the service; there's nothing
			 * else to do so just get rid of the frame which
			 * will as a side effect shut us down.
			 */
			frame.setVisible(false);
			frame.dispose();
			frame = null;
		    }
		}
	    });	    
	    d.pack();
	    d.show();
	}
    }

    /*
     * This class provides a transition dialog which allows the user
     * to initiate the address wizard immediately upon startup.  It's
     * done this way so that the startMeUp() method can use invokeLater()
     * to cause it to be displayed after the config wizard exit event
     * has been processed rather than during that event's processing;
     * otherwise the wizard doesn't disappear until after the user presses
     * Yes or No in this dialog.
     */
    class WizardTransition implements Runnable {
	public void run() {
	    // Now transition to configuring addresses
	    int status = JOptionPane.showConfirmDialog(frame,
		ResourceStrings.getString("start_address_wizard"),
		ResourceStrings.getString("start_address_wizard_title"),
		JOptionPane.YES_NO_OPTION);
	    if (status == JOptionPane.YES_OPTION) {
		addressView.startAddressWizard();
	    }
	}
    }

    // Create the frame within which the UI will live
    private void createFrame() {
	if (frame == null) {
	
	    frame = new MainFrame(ResourceStrings.getString("dhcp_manager"));
	    
	    // Create the views for this tool
	    if (modeIsRelay) {
		frame.addView(new RelayView(), true);
	    } else {
		addressView = new AddressView();
		frame.addView(addressView, true);
		frame.addView(new MacroView(), false);
		frame.addView(new OptionView(), false);
	    }
	    
	    // Set up the services menu
	    frame.addMenuAction(MainFrame.ACTIONS_MENU,
		(restartAction = new RestartAction()));
	    frame.addMenuAction(MainFrame.ACTIONS_MENU,
		(stopAction = new StopAction()));
	    frame.addMenuAction(MainFrame.ACTIONS_MENU,
		(startAction = new StartAction()));
	    frame.addMenuAction(MainFrame.ACTIONS_MENU,
		(disableAction = new DisableAction()));
	    frame.addMenuAction(MainFrame.ACTIONS_MENU,
		(enableAction = new EnableAction()));
	    frame.addMenuAction(MainFrame.ACTIONS_MENU,
		new ModifyServiceAction());
	    frame.addMenuAction(MainFrame.ACTIONS_MENU,
		new UnconfigureServiceAction());
	    
	    // Set up the Help menu
	    frame.addMenuAction(MainFrame.HELP_MENU, new OverviewAction());
	    frame.addMenuAction(MainFrame.HELP_MENU, new HowToAction());
	    frame.addMenuAction(MainFrame.HELP_MENU, new IndexAction());
	    frame.addMenuAction(MainFrame.HELP_MENU, new ServiceAction());
	    
	    // In relay mode, let it size itself (quite small)
	    if (modeIsRelay) {
		frame.pack();
	    } else {
		/*
		 * Normal mode set it to a reasonable size.  This ought to be
		 * a user preference, but until we run as something other than
		 * root it's not really a useful idea.
		 */
		frame.setSize(800, 600);
	    }
	    
	    // Listen for closing events
	    frame.addWindowListener(new WindowAdapter() {
		public void windowClosing(WindowEvent e) {
		    /*
		     * This is here to work around the Close selection frame
		     * menu on Solaris not causing the closed function to be
		     * called
		     */
		    windowClosed(e);
		}
		public void windowClosed(WindowEvent e) {
		    // Dispose of all data and exit when window goes away.
		    frame = null;
		    DataManager.get().reset();
		    requestExit();
		}
	    });
	}
    }
    
    // Show the frame
    private void showFrame() {
	if (frame == null) {
	    createFrame();
	}
	frame.initialize();
	if (modeIsRelay) {
	    // Disable edit & view menus in the relay case
	    frame.setMenuEnabled(MainFrame.EDIT_MENU, false);
	    frame.setMenuEnabled(MainFrame.VIEW_MENU, false);
	}
	try {
	    // Set status of service menu options based on server state
	    boolean running =
		DataManager.get().getDhcpServiceMgr().isServerRunning();
	    restartAction.setEnabled(running);
	    stopAction.setEnabled(running);
	    startAction.setEnabled(!running);
	    boolean enabled =
		DataManager.get().getDhcpServiceMgr().isServerEnabled();
	    enableAction.setEnabled(!enabled);
	    disableAction.setEnabled(enabled);
	} catch (Throwable e) {
	    // Enable all the menu items, as something went wrong
	    restartAction.setEnabled(true);
	    stopAction.setEnabled(true);
	    startAction.setEnabled(true);
	    enableAction.setEnabled(true);
	    disableAction.setEnabled(true);
	}    
	frame.show();
    }
    
    /*
     * main startup code; checks whether server is already configured, and if
     * not runs through the config wizard sequence in order to get the server
     * configured.
     */
    private void startMeUp() {
	try {
	    if (DataManager.get().getServer() == null) {
		DataManager.get().setServer(getCodeBase().getHost());
	    }
	    // See if server is already configured, and start up
	    DhcpdOptions opts =
		DataManager.get().getDhcpServiceMgr().readDefaults();
	    modeIsRelay = opts.isRelay();
	    // If server mode, ensure RESOURCE and PATH were set
	    if (!modeIsRelay) {
	    	if ((opts.getResource() == null) || (opts.getPath() == null)) {
		    System.err.println(
		        ResourceStrings.getString("err_initializing_options"));
	    	    requestExit();
		}
	    }
	    showFrame();
	} catch (BridgeException e) {
	    // Let user select which type of service to configure
	    int choice = ConfigureChoiceDialog.showDialog(frame);
	    if (choice == ConfigureChoiceDialog.DHCP) {
		// DHCP; run the wizard
		ConfigWizard wiz = new ConfigWizard(frame,
		    ResourceStrings.getString("cfg_wiz_title"), true);
		wiz.addActionListener(new ActionListener() {
		    public void actionPerformed(ActionEvent e) {
			if (e.getActionCommand().equals("finished")) {
			    // Service config completed, start up
			    modeIsRelay = false;
			    showFrame();
			    // Now transition to configuring addresses
			    SwingUtilities.invokeLater(new WizardTransition());
			} else {
			    // User cancelled the wizard, exit
			    requestExit();
			}
		    }
		});
		wiz.pack();
		wiz.setVisible(true);
	    } else if (choice == ConfigureChoiceDialog.BOOTP) {
		// Wants to configure a relay, show the dialog for that
		ConfigureRelayDialog d = new ConfigureRelayDialog(frame);
		d.addActionListener(new ActionListener() {
		    public void actionPerformed(ActionEvent e) {
			if (e.getActionCommand().equals(DialogActions.OK)) {
			    // Relay configuration completed, start up
			    modeIsRelay = true;
			    showFrame();
			} else {
			    // User cancelled, exit
			    requestExit();
			}
		    }
		});
		d.pack();
		d.show();
	    } else {
		// User cancelled; exit
		requestExit();
	    }
	} catch (Throwable e) {
	    // Couldn't really get started, dump the stack and exit
	    System.err.println(
	        ResourceStrings.getString("err_initializing_program"));
	    System.err.println(e.getMessage());
	    e.printStackTrace();
	    requestExit();
	}
    }

    // Show a help file referenced by tag
    public static void showHelp(String helpId) {
	// If help tag mapping table not loaded yet, then load it
	if (helpIds == null) {
	    try {
		helpIds = new HelpIds("com.sun.dhcpmgr.client.HelpBundle");
	    } catch (Throwable e) {
		// Error initializing help system
		JOptionPane.showMessageDialog(frame,
		    ResourceStrings.getString("err_initializing_help"),
		    ResourceStrings.getString("server_error_title"),
		    JOptionPane.ERROR_MESSAGE);
		return;
	    }
	}
	// Ask browser to display
	try {
	    Runtime.getRuntime().exec(
		    "/usr/dt/bin/netscape file:"
		    + helpIds.getFilePath(helpId));
	} catch (java.io.IOException e) {
	    JOptionPane.showMessageDialog(frame,
	    	ResourceStrings.getString("err_starting_help"),
		ResourceStrings.getString("server_error_title"),
		JOptionPane.ERROR_MESSAGE);
	}
    }
    
    // Exit the application
    private void requestExit() {
	System.exit(0);
    }
    
    // Main function when we're run as an application
    public static void main(String [] args) {

	// Ensure that we're running as root; exit if not
	if (!System.getProperty("user.name").equals("root")) {
	    System.err.println(ResourceStrings.getString("err_must_be_root"));
	    System.exit(0);
	}

	DhcpmgrApplet applet = new DhcpmgrApplet();
	applet.startMeUp();
    }
}
