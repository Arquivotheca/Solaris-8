/*
 * @(#)DhcptabMgrImpl.java	1.2	99/05/05 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.server;

import com.sun.dhcpmgr.bridge.*;
import com.sun.dhcpmgr.data.*;

/**
 * This class provides methods to manage the contents of the dhcptab.
 */

public class DhcptabMgrImpl implements DhcptabMgr {
    private Bridge bridge;
    
    /**
     * Create a new DhcptabMgr using the provided native bridge.
     * @param bridge the native bridge class which actually does the work.
     */
    public DhcptabMgrImpl(Bridge bridge) {
	this.bridge = bridge;
    }
    
    /**
     * Retrieve all options currently defined in the dhcptab.
     * @return an array of Options
     */
    public Option [] getOptions() throws BridgeException {
	return bridge.getOptions();
    }
    
    /**
     * Retrieve all the macros currently defined in the dhcptab.
     * @return an array of Macros
     */
    public Macro [] getMacros() throws BridgeException {
	/*
	 * Load the vendor and site options before loading the macros
	 * so we can validate correctly, adding them to the standard options
	 * table.
	 */
	OptionsTable optionsTable = OptionsTable.getTable();
	optionsTable.add(bridge.getOptions());
	return bridge.getMacros();
    }
    
    /**
     * Create a given record in the dhcptab, and signal the server to
     * reload the dhcptab if so requested.
     * @param rec the record to add to the table
     * @param signalServer true if the server is to be sent a SIGHUP
     */
    public void createRecord(DhcptabRecord rec, boolean signalServer)
	    throws BridgeException {
	bridge.createDhcptabRecord(rec);
	if (signalServer) {
	    bridge.reload();
	}
    }
    
    /**
     * Modify a given record in the dhcptab, and signal the server to reload
     * the dhcptab if so requested
     * @param oldRec the current record to modify
     * @param newRec the new record to be placed in the table
     * @param signalServer true if the server is to be sent a SIGHUP
     */
    public void modifyRecord(DhcptabRecord oldRec, DhcptabRecord newRec,
	    boolean signalServer) throws BridgeException {
	bridge.modifyDhcptabRecord(oldRec, newRec);
	if (signalServer) {
	    bridge.reload();
	}
    }
    
    /**
     * Delete a given record from the dhcptab, and signal the server to reload
     * the dhcptab if so requested
     * @param rec the record to delete
     * @param signalServer true if the server is to be sent a SIGHUP
     */
    public void deleteRecord(DhcptabRecord rec, boolean signalServer)
	    throws BridgeException {
	bridge.deleteDhcptabRecord(rec);
	if (signalServer) {
	    bridge.reload();
	}
    }
    
    /**
     * Create a new empty dhcptab in the server's data store, which must already
     * be configured.
     */
    public void createDhcptab() throws BridgeException {
	bridge.createDhcptab();
    }
    
    /**
     * Delete the server's dhcptab in the current data store.
     */
    public void deleteDhcptab() throws BridgeException {
	bridge.deleteDhcptab();
    }
}
