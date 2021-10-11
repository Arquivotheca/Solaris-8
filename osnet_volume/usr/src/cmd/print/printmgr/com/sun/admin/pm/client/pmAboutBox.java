/*
 *
 * ident	"@(#)pmAboutBox.java	1.2	99/03/29 SMI"
 *
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pmAboutBox.java
 * 
 */

package com.sun.admin.pm.client;

import javax.swing.*;

public class pmAboutBox {
    String title = pmUtility.getResource("About.Solaris.Print.Manager");
    String copyright = new String(pmUtility.getResource("info_copyright1") +
                                "\n" + 
                                pmUtility.getResource("info_copyright2"));
    String version = pmUtility.getResource("info_version");
    String appname = pmUtility.getResource("info_name");
    String build = pmUtility.getResource("info_build");

    String contents = new String(appname + "\n" +
                                  version + "\n" +
                                  build + "\n\n" +
                                  copyright + "\n");

    public pmAboutBox() {
        JOptionPane.showMessageDialog(null,
                                       contents,
                                       title,
                                       JOptionPane.INFORMATION_MESSAGE);
    }

}


