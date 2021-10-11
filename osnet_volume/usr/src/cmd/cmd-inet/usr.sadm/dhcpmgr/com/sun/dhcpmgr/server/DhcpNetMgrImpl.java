/*
 * @(#)DhcpNetMgrImpl.java	1.3	99/06/03 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.server;

import com.sun.dhcpmgr.bridge.*;
import com.sun.dhcpmgr.data.*;

/**
 * This class provides the functionality to manage the DHCP network tables and
 * the hosts table.
 */
public class DhcpNetMgrImpl implements DhcpNetMgr {
    private Bridge bridge;
    
    public DhcpNetMgrImpl(Bridge bridge) {
	this.bridge = bridge;
    }
    
    /**
     * Return the list of networks currently known to DHCP
     * @return an array of Networks
     */
    public Network [] getNetworks() throws BridgeException {
	return bridge.getNetworks();
    }
    
    /**
     * Return the list of addresses managed by DHCP on a given network
     * @param network the dotted-decimal representation of the network address
     * @return an array of records for the addresses defined on that network
     */
    public DhcpClientRecord [] loadNetwork(String network)
	    throws BridgeException {
	return bridge.loadNetwork(network.replace('.', '_'));
    }
    
    /**
     * Modify an existing client record, and update the associated hosts
     * record if needed.
     * @param oldClient the existing record
     * @param newClient the new record
     * @param table the network on which the record is defined
     */
    public synchronized void modifyClient(DhcpClientRecord oldClient,
	    DhcpClientRecord newClient, String table) throws BridgeException {

	// Update the network table record
	bridge.modifyDhcpClientRecord(oldClient, newClient,
	    table.replace('.', '_'));

	boolean nameChanged = !oldClient.getClientName().equals(
	    newClient.getClientName());
	boolean commentChanged = !oldClient.getComment().equals(
	    newClient.getComment());
	/*
	 * If the name changed, need to update hosts.  If comment changed,
	 * hosts is only updated if there was already a hosts record.
	 */
	if (nameChanged) {
	    /*
	     * If new name is empty, delete the hosts entry.  Otherwise
	     * try to modify it.
	     */
	    if (newClient.getClientName().length() == 0) {
	    	try {
		    bridge.deleteHostsRecord(newClient.getClientIPAddress());
		} catch (Throwable e) {
		    throw new NoHostsEntryException(
		        newClient.getClientIPAddress());
		}
	    } else {
	    	try {
		    bridge.modifyHostsRecord(oldClient.getClientIPAddress(),
		    	newClient.getClientIPAddress(),
			newClient.getClientName(), newClient.getComment());
	    	} catch (NoHostsEntryException e) {
		    // Must not be one, so create it instead
		    bridge.createHostsRecord(newClient.getClientIPAddress(),
		   	newClient.getClientName(), newClient.getComment());
	    	}
	    }
	} else if (commentChanged) {
	    // Try to modify, but toss all exceptions as this isn't a big deal
	    try {
	    	bridge.modifyHostsRecord(oldClient.getClientIPAddress(),
	    	    newClient.getClientIPAddress(), newClient.getClientName(),
		    newClient.getComment());
	    } catch (Throwable e) {
	    	// Ignore
	    }
	}
    }
    
    /**
     * Create a new record in the given table, and create a hosts record.
     * @param client the client to create
     * @param table the network on which to create the client
     */
    public synchronized void addClient(DhcpClientRecord client, String table)
	throws BridgeException {

	// Create the record in the per-network table
	bridge.createDhcpClientRecord(client, table.replace('.', '_'));

	/*
	 * If a name was supplied and we can't resolve it to this address,
	 * create a hosts record.
	 */
	if (client.getClientName().length() != 0
	    && !client.getClientName().equals(client.getClientIPAddress())) {
	    bridge.createHostsRecord(client.getClientIPAddress(),
		client.getClientName(), client.getComment());
	}
    }
    
    /**
     * Delete a record from the given table, and delete the associated hosts
     * record if requested.
     * @param client the client to delete
     * @param table the network to delete the client from
     * @param deleteHosts true if the hosts record should be removed as well
     */
    public synchronized void deleteClient(DhcpClientRecord client, String table,
	    boolean deleteHosts) throws BridgeException {

	// Delete the client record from the per-network table
	bridge.deleteDhcpClientRecord(client, table.replace('.', '_'));

	// Delete hosts if requested
	if (deleteHosts) {
	    try {
		bridge.deleteHostsRecord(client.getClientIPAddress());
	    } catch (NoEntryException e) {
		throw new NoEntryException("hosts");
	    }
	}
    }
    

    /**
     * Create a new per-network table for the given network.
     * @param network the network number in dotted-decimal form.
     */
    public synchronized void createNetwork(String network)
    	    throws BridgeException {
	String s = network.replace('.', '_');
	bridge.createDhcpNetwork(s);
    }
    
    /**
     * Delete a per-network table, the macro associated with the network number,
     * and optionally deleting the associated hosts records.
     * @param network the network number in dotted-decimal form.
     * @param deleteHosts true if the associated hosts records should be deleted
     */
    public synchronized void deleteNetwork(String network, boolean deleteHosts)
	    throws BridgeException {

	String s = network.replace('.', '_');

	// If we're supposed to clean up hosts, do so
	if (deleteHosts) {
	    DhcpClientRecord [] recs = bridge.loadNetwork(s);
	    if (recs != null) {
		for (int i = 0; i < recs.length; ++i) {
		    try {
			bridge.deleteHostsRecord(recs[i].getClientIPAddress());
		    } catch (Throwable e) {
			// Ignore errors here; they're not important
		    }
		}
	    }
	}

	// Delete network table, then the macro for the network
	bridge.deleteDhcpNetwork(s);
	try {
	    bridge.deleteDhcptabRecord(new Macro(network));
	} catch (Throwable e) {
	    // All the errors here are ignorable
	}
    }
}
