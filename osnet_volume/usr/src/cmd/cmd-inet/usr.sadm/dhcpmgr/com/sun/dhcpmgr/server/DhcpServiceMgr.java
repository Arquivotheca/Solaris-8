/*
 * @(#)DhcpServiceMgr.java	1.2	99/10/12 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.server;

import com.sun.dhcpmgr.bridge.*;
import com.sun.dhcpmgr.data.*;

/**
 * This interface defines the methods available for managing the basic
 * service parameters which are not stored in the dhcptab or network tables.
 */
public interface DhcpServiceMgr {
    public DhcpDatastore [] getDataStores() throws BridgeException;
    public DhcpdOptions readDefaults() throws BridgeException;
    public void writeDefaults(DhcpdOptions defs) throws BridgeException;
    public void removeDefaults() throws BridgeException;
    public void createLinks() throws BridgeException;
    public void removeLinks() throws BridgeException;
    public void startup() throws BridgeException;
    public void shutdown() throws BridgeException;
    public void reload() throws BridgeException;
    public IPInterface [] getInterfaces() throws BridgeException;
    public String getStringOption(short code, String arg)
	throws BridgeException;
    public IPAddress [] getIPOption(short code, String arg)
	throws BridgeException;
    public long [] getNumberOption(short code, String arg)
	throws BridgeException;
    public boolean isServerRunning() throws BridgeException;
    public boolean isServerEnabled() throws BridgeException;
}
