/*
 * @(#)SelectionListener.java	1.1	99/03/22 SMI
 *
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
package com.sun.dhcpmgr.ui;

import java.util.EventListener;

public interface SelectionListener extends EventListener {
    public void valueChanged();
}
