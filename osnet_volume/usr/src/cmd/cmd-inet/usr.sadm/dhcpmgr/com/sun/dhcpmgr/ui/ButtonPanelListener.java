/*
 * @(#)ButtonPanelListener.java	1.1	99/03/22 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.ui;

public interface ButtonPanelListener {
    public final static int OK = 0;
    public final static int CANCEL = 1;
    public final static int HELP = 2;
    public final static int RESET = 3;
    
    public void buttonPressed(int buttonId);
}
