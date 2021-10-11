/*
 * @(#)DhcptabRecord.java	1.1 99/03/22 SMI
 *
 * Copyright (c) 1998 Sun Microsystems, Inc.
 * All rights reserved.
 */

package com.sun.dhcpmgr.data;

import java.io.Serializable;

public abstract class DhcptabRecord implements Serializable, Cloneable {
    public static final String MACRO = "m";
    public static final String OPTION = "s";
    protected String key;
    protected String flag;
    protected String value;
    
    public DhcptabRecord() {
	key = flag = value = "";
    }
    
    public DhcptabRecord(String k, String f, String v) {
	key = k;
	flag = f;
	value = v;
    }
    
    public void setKey(String k) throws ValidationException {
	key = k;
    }
    
    public String getKey() {
	return key;
    }
    
    public void setFlag(String f) throws ValidationException {
	flag = f;
    }
    
    public String getFlag() {
	return flag;
    }
    
    public void setValue(String v) throws ValidationException {
	value = v;
    }
    
    public String getValue() {
	return value;
    }
    
    public String toString() {
	return new String(key + " " + flag + " " + value);
    }
}
