/*
 * @(#)OptionValue.java	1.1	99/03/22 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.data;

import java.util.Vector;

public interface OptionValue extends Cloneable {    
    public String getName();
    public String getValue();
    public void setValue(Object value) throws ValidationException;
    public boolean isValid();
    public Object clone();
}
