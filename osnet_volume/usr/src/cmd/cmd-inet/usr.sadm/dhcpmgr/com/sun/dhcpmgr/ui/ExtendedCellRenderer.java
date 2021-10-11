/*
 * @(#)ExtendedCellRenderer.java	1.2	99/05/12 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.ui;

import javax.swing.table.DefaultTableCellRenderer;
import java.util.Date;
import java.text.DateFormat;
import java.net.InetAddress;
import com.sun.dhcpmgr.data.IPAddress;

// Renderer for cells containing Dates, InetAddresses or IPAddresses
public class ExtendedCellRenderer extends DefaultTableCellRenderer {
    private DateFormat dateFormat = DateFormat.getInstance();

    protected void setValue(Object value) {
	if (value != null) {
	    if (value instanceof Date) {
		long t = ((Date)value).getTime();
		if (t == 0) {
		    super.setValue(null);
		} else if (t < 0) {
		    super.setValue(ResourceStrings.getString("never"));
		} else {
		    super.setValue(dateFormat.format(value));
		}
	    } else if (value instanceof InetAddress) {
		super.setValue(((InetAddress)value).getHostAddress());
	    } else if (value instanceof IPAddress) {
		super.setValue(((IPAddress)value).getHostAddress());
	    } else {
		super.setValue(value);
	    }
	} else {
	    super.setValue(value);
	}
    }
}
