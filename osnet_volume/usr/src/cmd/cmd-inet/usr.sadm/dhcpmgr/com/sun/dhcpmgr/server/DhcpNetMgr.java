/*
 * @(#)DhcpNetMgr.java	1.1	99/03/22 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.server;

import com.sun.dhcpmgr.bridge.*;
import com.sun.dhcpmgr.data.*;

/**
 * This class defines the methods available to manage the DHCP network
 * tables and hosts table.
 */
public interface DhcpNetMgr {
    public Network [] getNetworks() throws BridgeException;
    public DhcpClientRecord [] loadNetwork(String network)
	throws BridgeException;
    public void modifyClient(DhcpClientRecord oldClient,
	DhcpClientRecord newClient, String table)
	throws BridgeException;
    public void addClient(DhcpClientRecord client, String table)
	throws BridgeException;
    public void deleteClient(DhcpClientRecord client, String table,
	boolean deleteHosts) throws BridgeException;
    public void createNetwork(String network) throws BridgeException;
    public void deleteNetwork(String network, boolean deleteHosts)
	throws BridgeException;
}
