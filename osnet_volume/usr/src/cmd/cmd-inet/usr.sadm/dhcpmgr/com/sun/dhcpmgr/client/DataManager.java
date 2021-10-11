/*
 * @(#)DataManager.java	1.2	99/04/29 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.client;

import java.net.InetAddress;
import java.net.UnknownHostException;

import com.sun.dhcpmgr.bridge.*;
import com.sun.dhcpmgr.data.*;
import com.sun.dhcpmgr.server.*;

/**
 * DataManager implements a central point of contact between the client and the
 * server components; the client obtains all server object references through
 * this class which allows us to cache information as much as possible, 
 * improving users' perception of the performance of the tool.
 */
public class DataManager {
    private DhcpMgr server;
    private DhcpNetMgr netMgr;
    private DhcptabMgr dhcptabMgr;
    private DhcpServiceMgr dhcpServiceMgr;
    private Network [] networks;
    private DhcpClientRecord [] clients;
    private String clientNet;
    private Macro [] macros;
    private Option [] options;
    private String serverName;
    private String shortServerName;
    private InetAddress serverAddress;
    private static DataManager mgr = null;
    
    private DataManager() {
	reset();
    }
    
    public synchronized void reset() {
	// Clear the data references; used by the applet to re-initialize
	server = new DhcpMgrImpl();
	netMgr = null;
	dhcptabMgr = null;
	dhcpServiceMgr = null;
	/*
	 * The arrays aren't nulled so that we can use them as the lock objects
	 * in the synchronized blocks below
	 */
	networks = new Network[0];
	clients = new DhcpClientRecord[0];
	clientNet = null;
	macros = new Macro[0];
	options = new Option[0];
	try {
	    serverAddress = InetAddress.getLocalHost();
	    setServerName(serverAddress.getHostName());
	} catch (UnknownHostException e) {
	    serverName = shortServerName = "";
	}
    }
    
    private void setServerName(String name) {
	serverName = name;
	int i = serverName.indexOf('.');
	if (i == -1) {
	    shortServerName = serverName;
	} else {
	    shortServerName = serverName.substring(0, i);
	}
    }

    /*
     * Threading note: the following methods are all synchronized on the
     * class so that server references and names are always set & retrieved
     * in a consistent state.
     */
    public synchronized static DataManager get() {
	if (mgr == null) {
	    mgr = new DataManager();
	}
	return mgr;
    }
    
    public synchronized void setServer(String name) throws Exception {
	setServerName(name);
	serverAddress = null;
    }
    
    public synchronized DhcpMgr getServer() {
	return server;
    }
    
    public synchronized String getServerName() {
	return serverName;
    }
    
    public synchronized String getShortServerName() {
	return shortServerName;
    }

    public synchronized InetAddress getServerAddress() {
	return serverAddress;
    }

    public synchronized DhcpNetMgr getDhcpNetMgr() {
	if (netMgr == null) {
	    netMgr = getServer().getNetMgr();
	}
	return netMgr;
    }
    
    public synchronized DhcptabMgr getDhcptabMgr() {
	if (dhcptabMgr == null) {
	    dhcptabMgr = getServer().getDhcptabMgr();
	}
	return dhcptabMgr;
    }
    
    public synchronized DhcpServiceMgr getDhcpServiceMgr() {
	if (dhcpServiceMgr == null) {
	    dhcpServiceMgr = getServer().getDhcpServiceMgr();
	}
	return dhcpServiceMgr;
    }
    
    /*
     * End of class-synchronized methods.  Remaining methods are synchronized
     * at a data item level.
     */

    public Network [] getNetworks(boolean forceUpdate) throws BridgeException {
	synchronized (networks) {
	    if (forceUpdate || networks.length == 0) {
		networks = getDhcpNetMgr().getNetworks();
		if (networks == null) {
		    networks = new Network[0];
		}
	    }
	}
	return networks;
    }
    
    public DhcpClientRecord [] getClients(String net, boolean forceUpdate)
	    throws BridgeException {
	synchronized (clients) {
	    if (forceUpdate || clients.length == 0 || !net.equals(clientNet)) {
		clients = getDhcpNetMgr().loadNetwork(net);
		if (clients == null) {
		    clients = new DhcpClientRecord[0];
		}
		clientNet = net;
	    }
	}
	return clients;
    }
    
    public Macro [] getMacros(boolean forceUpdate) throws BridgeException {
	synchronized (macros) {
	    if (forceUpdate || macros.length == 0) {
		macros = getDhcptabMgr().getMacros();
		if (macros == null) {
		    macros = new Macro[0];
		}
	    }
	}
	return macros;
    }
    
    public Option [] getOptions(boolean forceUpdate) throws BridgeException {
	synchronized (options) {
	    if (forceUpdate || options.length == 0) {
		options = getDhcptabMgr().getOptions();
		if (options == null) {
		    options = new Option[0];
		}
		/*
		 * Reload the site/vendor options portion of the global options
		 * table with the data we just loaded so that macro validation
		 * can be handled properly.
		 */
		OptionsTable.getTable().add(options); 
	    }
	}
	return options;
    }
}
