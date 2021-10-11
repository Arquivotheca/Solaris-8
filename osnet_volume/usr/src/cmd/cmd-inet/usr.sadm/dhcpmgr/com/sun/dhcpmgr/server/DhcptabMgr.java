/*
 * @(#)DhcptabMgr.java	1.1	99/03/22 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.server;

import com.sun.dhcpmgr.bridge.*;
import com.sun.dhcpmgr.data.*;

/**
 * This interface defines the methods available for managing the dhcptab.
 */
public interface DhcptabMgr {
    public Option [] getOptions() throws BridgeException;
    public Macro [] getMacros() throws BridgeException;
    public void createRecord(DhcptabRecord rec, boolean signalServer)
	throws BridgeException;
    public void modifyRecord(DhcptabRecord oldRec, DhcptabRecord newRec,
	boolean signalServer) throws BridgeException;
    public void deleteRecord(DhcptabRecord rec, boolean signalServer)
	throws BridgeException;
    public void createDhcptab() throws BridgeException;
    public void deleteDhcptab() throws BridgeException;
}
