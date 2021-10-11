/*
 * @(#)DhcpDatastore.java	1.1 99/03/22 SMI
 *
 * Copyright (c) 1998 Sun Microsystems, Inc.
 * All rights reserved.
 */

package com.sun.dhcpmgr.data;

import java.io.Serializable;

public class DhcpDatastore implements Serializable {
    public static final int UFS = 0;
    public static final int NISPLUS = 1;
    
    private int code;
    private String name;
    
    public DhcpDatastore() {
	code = -1;
	name = null;
    }
    
    DhcpDatastore(int c, String s) {
	code = c;
	name = s;
    }
    
    public int getCode() {
	return code;
    }
    
    public void setCode(int c) {
	code = c;
    }
    
    public String getName() {
	return name;
    }
    
    public void setName(String s) {
	name = s;
    }
}
