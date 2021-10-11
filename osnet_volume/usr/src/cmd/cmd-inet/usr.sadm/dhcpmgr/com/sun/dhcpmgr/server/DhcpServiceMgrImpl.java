/*
 * @(#)DhcpServiceMgrImpl.java	1.3	99/10/12 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.server;

import com.sun.dhcpmgr.bridge.*;
import com.sun.dhcpmgr.data.*;

/**
 * This class provides the capabilities for managing the the basic service
 * parameters which are not stored in the dhcptab or per-network tables.
 */
public class DhcpServiceMgrImpl implements DhcpServiceMgr {
    private Bridge bridge;
    
    public DhcpServiceMgrImpl(Bridge bridge) {
	this.bridge = bridge;
    }
    
    /**
     * Retrieve the list of possible data stores for this server
     * @return an array of DhcpDatastore's
     */
    public DhcpDatastore [] getDataStores() throws BridgeException {
	return bridge.getDataStores();
    }
    
    /**
     * Retrieve the contents of the server's defaults file /etc/default/dhcp.
     * @return the defaults settings
     */
    public DhcpdOptions readDefaults() throws BridgeException {
	return bridge.readDefaults();
    }
    
    /**
     * Write new settings to the server's defaults file /etc/default/dhcp.
     * @param defs the new defaults settings
     */
    public void writeDefaults(DhcpdOptions defs) throws BridgeException {
	bridge.writeDefaults(defs);
    }
    
    /**
     * Remove the server's defaults file /etc/default/dhcp
     */
    public void removeDefaults() throws BridgeException {
	bridge.removeDefaults();
    }
    
    /**
     * Create the links to /etc/init.d/dhcp for the various run-levels
     */
    public void createLinks() throws BridgeException {
	bridge.createLinks();
    }
    
    /**
     * Remove the links to /etc/init.d/dhcp for the various run-levels
     */
    public void removeLinks() throws BridgeException {
	bridge.removeLinks();
    }
    
    /**
     * Start the server
     */
    public void startup() throws BridgeException {
	bridge.startup();
    }
    
    /**
     * Stop the server
     */
    public void shutdown() throws BridgeException {
	bridge.shutdown();
    }
    
    /**
     * Send the server a SIGHUP to re-read the dhcptab
     */
    public void reload() throws BridgeException {
	bridge.reload();
    }
    
    /**
     * Get the list of possible interfaces for the server to monitor
     * @return an array of interfaces
     */
    public IPInterface [] getInterfaces() throws BridgeException {
	return bridge.getInterfaces();
    }
    
    /**
     * Get the default value for an option which would take a string
     * @param code the option code
     * @param arg additional information needed for this code
     */
    public synchronized String getStringOption(short code, String arg)
	    throws BridgeException {
	return bridge.getStringOption(code, arg);
    }
    
    /**
     * Get the default value for an option which would take one or more IP addrs
     * @param code the option code
     * @param arg additional information needed for this code
     */
    public synchronized IPAddress [] getIPOption(short code, String arg)
	    throws BridgeException {
	return bridge.getIPOption(code, arg);
    }
    
    /**
     * Get the default value for an option which would take one or more numbers
     * @param code the option code
     * @param arg additional information needed for this code
     */
    public synchronized long [] getNumberOption(short code, String arg)
	    throws BridgeException {
	return bridge.getNumberOption(code, arg);
    }
    
    /**
     * Check if the server is currently running
     * @return true if the server process is started
     */
    public boolean isServerRunning() throws BridgeException {
	return bridge.isServerRunning();
    }
    
    /**
     * Check if the server is enabled to start at boot time
     * @return true if the server is enabled
     */
    public boolean isServerEnabled() throws BridgeException {
	return bridge.isServerEnabled();
    }
}
