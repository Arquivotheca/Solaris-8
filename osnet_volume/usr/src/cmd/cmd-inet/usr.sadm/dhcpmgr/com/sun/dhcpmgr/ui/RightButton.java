/*
 * @(#)RightButton.java	1.1	99/03/22 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.ui;

/**
 * Class to produce a button with an icon pointing right
 */
public class RightButton extends ImageButton {
    public RightButton() {
	setImage(getClass(), "next.gif", ">>");
    }
}
