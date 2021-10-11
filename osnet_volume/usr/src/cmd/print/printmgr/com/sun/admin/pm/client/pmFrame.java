/*
 *
 * ident	"@(#)pmFrame.java	1.6	99/09/24 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmFrame.java
 * Extends JFrame to support better focus handling
 */

package com.sun.admin.pm.client;

import java.awt.*;
import java.awt.event.*;
import java.net.*;
import javax.swing.*;

import com.sun.admin.pm.server.*;

class pmFrame extends JFrame {

    // this comp gets focus on frame open
    Component defaultComponent = null;

    // file path of icon; this does not provide for localization
    private static final String iconName = "images/appicon.gif";
    
    // if true, clean up pmButton state on frame close
    private boolean clearButtonsOnClose;

    public pmFrame(String s) {
        super(s);

        // default: do NOT clear default button state when frame is closed
        clearButtonsOnClose = false;

        this.addFocusListener(new FocusListener() {
            public void focusGained(FocusEvent e) {
                Debug.message("CLNT:  pmFrame focus gained: " + e);
                if (defaultComponent != null) {
                    Debug.message("CLNT:  pmFrame focus to default comp");
                    defaultComponent.requestFocus();
                } else
                    Debug.message("CLNT:  pmFrame no default comp");
            }
            public void focusLost(FocusEvent e) {
                Debug.message("CLNT:  frame focus lost: " + e);
            }
        });

        this.addWindowListener(new WindowAdapter() {
            public void windowClosing(WindowEvent e) {
                Debug.info("frame Window closing");
                cleanupButtons();
            }
            public void windowClosed(WindowEvent e) {
                Debug.info("frame Window closed");
                cleanupButtons();
            }
        });

        try {
            Class thisClass = this.getClass();
            URL iconUrl = thisClass.getResource(iconName);
	    // System.out.println("Icon: " + iconUrl);
            if (iconUrl == null)
                Debug.warning("Unable to resolve URL for icon " + iconName);
            else {
                Toolkit tk = Toolkit.getDefaultToolkit();
                Image img = tk.getImage(iconUrl);
                this.setIconImage(img);
            }

        } catch (Exception x) {
            Debug.warning(x.toString());
        }
    }


    public void setDefaultComponent(Component c) {
        defaultComponent = c;
    }


    // If the frame is a minimized icon, make it un-minimized first
    public void setVisible(boolean isVisible) {
        if (isVisible == true) {
	    try {
		// this will fail in jdk 1.1 but work fine in 1.2
		setState(NORMAL);
	    } catch (Exception ssx) {
		// restores an iconified window in JDK 1.1
		removeNotify();
		addNotify();
	    }
        } else
            cleanupButtons();
        super.setVisible(isVisible);
    }

    public void cleanupButtons() {
        // drop this rootPane from pmButton's hashtable
        if (clearButtonsOnClose)
            pmButton.unreference(this.getRootPane());
    }

    protected void setClearButtonsOnClose(boolean b) {
        clearButtonsOnClose = b;
    }


}


