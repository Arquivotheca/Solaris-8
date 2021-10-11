/*
 * @(#)DhcpMgrImpl.java	1.1	99/03/22 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.server;

import com.sun.dhcpmgr.bridge.*;

public class DhcpMgrImpl implements DhcpMgr {
    private Bridge bridge;
    private DhcpNetMgrImpl netMgr;
    private DhcptabMgrImpl dtMgr;
    private DhcpServiceMgrImpl srvMgr;
    
    public DhcpMgrImpl() {
	bridge = new Bridge();
    }
    
    public DhcpNetMgr getNetMgr()  {
	if (netMgr == null) {
	    netMgr = new DhcpNetMgrImpl(bridge);
	}
	return netMgr;
    }
    
    public DhcptabMgr getDhcptabMgr()  {
	if (dtMgr == null) {
	    dtMgr = new DhcptabMgrImpl(bridge);
	}
	return dtMgr;
    }
    
    public DhcpServiceMgr getDhcpServiceMgr()  {
	if (srvMgr == null) {
	    srvMgr = new DhcpServiceMgrImpl(bridge);
	}
	return srvMgr;
    }
}
