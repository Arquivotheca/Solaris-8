/*
 * @(#)Bridge.java	1.5 99/10/12 SMI
 *
 * Copyright (c) 1998-1999 Sun Microsystems, Inc.
 * All rights reserved.
 */

package com.sun.dhcpmgr.bridge;

import java.util.Hashtable;
import com.sun.dhcpmgr.data.*;

/**
 * Bridge supplies access to the native functions in libdhcp and the
 * dhcpmgr shared object which actually interact with the data stores
 * used by DHCP.
 */

public class Bridge {
    public native DhcpDatastore [] getDataStores() throws BridgeException;
    public native DhcptabRecord getDhcptabRecord(String key, String flag)
	throws BridgeException;
    public native void createDhcptabRecord(DhcptabRecord rec)
	throws BridgeException;
    public native void modifyDhcptabRecord(DhcptabRecord oldRecord,
	DhcptabRecord newRecord) throws BridgeException;
    public native void deleteDhcptabRecord(DhcptabRecord rec)
	throws BridgeException;
    public native Option [] getOptions() throws BridgeException;
    public native Macro [] getMacros() throws BridgeException;
    public native Network [] getNetworks() throws BridgeException;
    public native DhcpClientRecord [] loadNetwork(String table)
	throws BridgeException;
    public native void createDhcpClientRecord(DhcpClientRecord rec,
	String table) throws BridgeException;
    public native void modifyDhcpClientRecord(DhcpClientRecord oldRecord,
	DhcpClientRecord newRecord, String table) throws BridgeException;
    public native void deleteDhcpClientRecord(DhcpClientRecord rec,
	String table) throws BridgeException;
    public native DhcpdOptions readDefaults() throws BridgeException;
    public native void writeDefaults(DhcpdOptions defs) throws BridgeException;
    public native void removeDefaults() throws BridgeException;
    public native void createLinks() throws BridgeException;
    public native void removeLinks() throws BridgeException;
    public native void startup() throws BridgeException;
    public native void shutdown() throws BridgeException;
    public native void reload() throws BridgeException;
    public native IPInterface [] getInterfaces() throws BridgeException;
    public native String getStringOption(short code, String arg)
	throws BridgeException;
    public native IPAddress [] getIPOption(short code, String arg)
	throws BridgeException;
    public native long [] getNumberOption(short code, String arg)
	throws BridgeException;
    public native void createHostsRecord(String address, String name,
	String comment) throws BridgeException;
    public native void modifyHostsRecord(String oldAddress, String newAddress,
	String name, String comment) throws BridgeException;
    public native void deleteHostsRecord(String address) throws BridgeException;
    public native void createDhcptab() throws BridgeException;
    public native void deleteDhcptab() throws BridgeException;
    public native void createDhcpNetwork(String net) throws BridgeException;
    public native void deleteDhcpNetwork(String net) throws BridgeException;
    public native boolean isServerRunning() throws BridgeException;
    public native boolean isServerEnabled() throws BridgeException;
    static {
	System.load("/usr/sadm/admin/dhcpmgr/dhcpmgr.so.1");
    }
}
