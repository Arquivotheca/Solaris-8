/*
 * @(#)DhcpMgr.java	1.2	99/04/29 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.server;

public interface DhcpMgr {
    public DhcpNetMgr getNetMgr();
    public DhcptabMgr getDhcptabMgr();
    public DhcpServiceMgr getDhcpServiceMgr();
}
