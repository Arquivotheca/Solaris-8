/*
 * @(#)LeftButton.java	1.1	99/03/22 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.ui;

/**
 * A button with an icon pointing left
 */
public class LeftButton extends ImageButton {
    
    public LeftButton() {
	setImage(getClass(), "back.gif", "<<");
    }
}
