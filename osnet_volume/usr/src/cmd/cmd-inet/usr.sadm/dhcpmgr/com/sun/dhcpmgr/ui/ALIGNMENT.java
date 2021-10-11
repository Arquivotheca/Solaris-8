/*
 * @(#)ALIGNMENT.java	1.1	99/03/22 SMI
 *
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.ui;

/**
 * Enumeration for <CODE>ALIGNMENT</CODE> values.
 */
public class ALIGNMENT {

	public static final ALIGNMENT CENTER = new ALIGNMENT();
	public static final ALIGNMENT LEFT   = new ALIGNMENT();
	public static final ALIGNMENT RIGHT  = new ALIGNMENT();
	public static final ALIGNMENT TOP    = new ALIGNMENT();
	public static final ALIGNMENT BOTTOM = new ALIGNMENT();

	private ALIGNMENT() { }
}
